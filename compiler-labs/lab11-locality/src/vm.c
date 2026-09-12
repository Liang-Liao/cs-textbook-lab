/*
 * vm.c —— 栈式虚拟机：把 lab6 里"C 递归兼任的调用栈"摊开成看得见的
 *         活动记录（本 lab 主角；对照龙书 7.1~7.4）
 *
 * 【机器模型】一台只有五样东西的机器：
 *     globals[]  全局数据区        stack[]   运行时栈（向高地址生长）
 *     sp         栈顶游标          fp        当前帧基址（帧指针）
 *     acc        累加器——函数返回值的唯一通道，相当于真实 CPU 的 rax
 * 指令集就是 lab6 的四元式，一条不改——变的只是"怎么执行 call/return"。
 *
 * 【存储组织】(7.1 节) 顶层声明的变量是全局变量，住 globals[]；每次
 * 函数调用在 stack[] 上压一个**活动记录**（activation record / 帧）：
 *
 *      低地址
 *    ┌──────────────┐
 *    │ 实参 n-1..0  │ caller 的 param 压好，call 后原样留给被调方
 *    ├──────────────┤
 *    │ 返回 pc      │ ← fp+2  回到 caller 的下一条指令
 *    │ 返回段号     │ ← fp+1  跨段跳转得连"回到哪一段"一起记
 *    │ 旧 fp        │ ← fp+0  fp 链：顺它能一路摸回所有祖先帧
 *    ├──────────────┤ ← fp（被调方帧基址）
 *    │ 局部槽 ...   │ ← fp+3 起：形参占 0..nparams-1，其后是其他局部
 *    └──────────────┘ ← sp
 *      高地址
 *
 * 【装载期：名字 → 槽位】gen 已保证 IR 名全局唯一（同名加 @N 后缀）、
 * 临时编号全程序不复位，于是两级寻址可以纯静态推导：
 *   - funcs[0]（主段）里被赋值过的名字 → 全局区各占一格；
 *   - 函数段里其余名字 → 本函数帧内的偏移（形参先占 0..n-1）。帧大小
 *     编译期即知——这正是第 7 章反复强调的"相对 fp 的偏移编译期确定，
 *     运行期只做 stack[fp+off]"。对比 lab6 的 irvm：每读一个变量都要
 *     沿帧链 strcmp 一圈；这里装载期定槽、执行期 O(1) 下标。真实编译
 *     器里"符号 → 帧偏移"的绑定就发生在这一步。
 *   - 数组（6.4）：arrays[] 描述符里的每个数组在全局区或帧内占连续
 *     len 格，NameSlot.off 记基址槽——Q_LDX/Q_STX 按"基址 + 下标"
 *     取元素；新帧清零的不变量同时保证数组的声明即零初始化。
 *
 * 【调用序列】(7.2 节 calling sequence)
 *   caller: param ×n —— 实参依次压栈；
 *           call f,n —— 压(旧fp, 自身段号, pc+1)，fp=sp，跳到 f 开头，
 *                       序言把实参从栈上拷进帧内形参槽（-O0 的真编译
 *                       器也这么干），局部区清零；
 *   callee: ……执行……
 *           return y —— acc = y（无 y 取返回类型零值），按 fp 收帧，
 *                       弹返回地址跳回。
 *   实参区的清理归 **caller**（cdecl 风格"调用者清栈"）：回去落点的
 *   前一条恰是发起调用的那条 call（VM 不变式：return 只落在 call+1），
 *   argc 就在那条指令里。换成 callee 清栈是 README 练习 1。
 *   递归从此不靠 C 递归：每层调用只是往栈上多压一个帧，fib(10) 的十
 *   层 fib_n 各在各的帧里——tests/cases/vm.mc 专测这一点。
 *
 * 【为什么没有 static link】(7.3 节) MiniC 无嵌套函数：函数只看得到
 *   全局区和自己这帧，不需要访问调用者的局部——fp 链只用于调试展示
 *   （-trace），不参与寻址。
 *
 * 求值语义与 eval.c/irvm.c 逐字一致（int/int 保 int、任一 float 提升、
 * 比较得 int、print %ld/%g）——run_tests.sh 三引擎对拍验证。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"

/* ---------------- 0. 运行时值与机器状态（Val 与 irvm.c 一致） ---------------- */

typedef struct {
    int    isfloat;
    long   i;
    double d;
} Val;

#define STACK_MAX  65536    /* 运行时栈容量（Val 槽）；超出 = 栈溢出 */
#define GLOBAL_MAX 4096     /* 全局数据区容量 */
#define FRAME_HDR  3        /* 帧头三格：旧 fp / 返回段号 / 返回 pc */
#define MAX_LABELS 4096
#define MAX_SLOTS  512      /* 单段名字数上限 */
#define MAX_FUNCS  128      /* 同 IrProgram.funcs 的容量 */

static Val g_stack[STACK_MAX];
static Val g_globals[GLOBAL_MAX];
static int sp;              /* 下一空闲槽下标 */
static int fp;              /* 当前帧基址 */
static Val acc;             /* 累加器：返回值经它传递，不过栈 */
static int g_depth;         /* 当前调用深度（-trace 展示用） */
static int g_trace;

/* ---------------- 1. 装载期：名字 → 槽位 ----------------
 * 每个 IR 名要么分到全局区一格（is_global=1，off 是 globals 下标），
 * 要么分到某函数帧内一格（is_global=0，off 是相对 fp+FRAME_HDR 的
 * 偏移）。装载期分拣完毕后，执行期不再有任何字符串查找。 */

typedef struct {
    const char *name;
    int         is_global;
    int         off;
} NameSlot;

typedef struct {
    NameSlot names[MAX_SLOTS];
    int      nnames;
    int      nslots;    /* 已分配的槽数：主段 = 全局区大小；
                         * 函数段 = 帧内局部格总数（含形参，即帧大小） */
    int      idx_by_label[MAX_LABELS];   /* 标号号 → 指令下标（同 irvm） */
} FuncInfo;

static FuncInfo g_info[MAX_FUNCS];

static int slot_find(const FuncInfo *fi, const char *name)
{
    for (int k = 0; k < fi->nnames; k++)
        if (strcmp(fi->names[k].name, name) == 0) return k;
    return -1;
}

/* 名字是否是数组；是则返回描述符（长度/元素类型/全局归属） */
static const ArrayDesc *arr_of(const IrProgram *p, const char *name)
{
    for (int i = 0; i < p->narrays; i++)
        if (strcmp(p->arrays[i].name, name) == 0) return &p->arrays[i];
    return NULL;
}

/* 登记一个名字到下一空槽（重复登记由调用方过滤） */
static void slot_add(FuncInfo *fi, const char *name, int is_global)
{
    NameSlot *s = &fi->names[fi->nnames++];
    s->name      = name;
    s->is_global = is_global;
    s->off       = fi->nslots++;
}

/* 这些 op 的 x 会写入一个名字（Q_CALL 的 x 可为 NULL——void 调用；
 * Q_LDX 的 x 是数组读的结果临时，同样是标量写入点） */
static const char *dest_of(const Quad *q)
{
    switch (q->op) {
    case Q_ASSIGN: case Q_BINOP: case Q_NEG: case Q_NOT:
    case Q_I2F: case Q_LDX: case Q_CALL:
        return q->x;
    default:
        return NULL;
    }
}

static void load_program(const IrProgram *p)
{
    /* 主段先行：它的名字表就是全局数据区的布局。顶层声明的变量在
     * gen 里落进主段，所以"主段赋过值的名字"恰好 = 全部全局变量 +
     * 主段的临时（临时编号全程序不复位，不会和函数里的撞名）。 */
    FuncInfo *m = &g_info[0];
    const IrFunc *f0 = &p->funcs[0];
    m->nnames = m->nslots = 0;
    for (int i = 0; i < MAX_LABELS; i++) m->idx_by_label[i] = -1;

    /* 全局数组先在全局数据区铺好连续 len 格——主段的标量扫描看到
     * 同名已登记会自动跳过（6.4：数组占一整块，不是单格名字） */
    for (int i = 0; i < p->narrays; i++) {
        const ArrayDesc *d = &p->arrays[i];
        if (!d->is_global) continue;
        if (m->nslots + d->len > GLOBAL_MAX || m->nnames >= MAX_SLOTS) {
            fprintf(stderr, "internal error: 全局区容纳不下数组 %s\n", d->name);
            exit(2);
        }
        NameSlot *s = &m->names[m->nnames++];
        s->name = d->name;
        s->is_global = 1;
        s->off  = m->nslots;        /* 基址槽；元素 i 在 off+i */
        m->nslots += d->len;
    }

    for (int i = 0; i < f0->n; i++) {
        const Quad *q = &f0->q[i];
        if (q->op == Q_LABEL && q->label < MAX_LABELS)
            m->idx_by_label[q->label] = i;
        const char *dst = dest_of(q);
        if (dst && slot_find(m, dst) < 0) {
            /* 名字表和全局数据区是两种资源，都要查——旧版只查了
             * nslots（数据区），nnames 打满后 slot_add 越界写 names[]。 */
            if (m->nnames >= MAX_SLOTS || m->nslots >= GLOBAL_MAX) {
                fprintf(stderr, "internal error: 全局区名字超过 %d 个"
                                "或容量超限\n", MAX_SLOTS);
                exit(2);
            }
            slot_add(m, dst, 1);
        }
    }

    /* 各函数段：形参先占帧内槽 0..nparams-1，再扫全段指令，凡不在
     * 全局区的名字编进帧内槽。操作数也扫一遍属防御性登记——正常程
     * 序里每个名字必先作为目标出现（decl 发 x=0），这里保证即便有
     * 漏网之鱼读到的也是零值而非野内存。 */
    if (p->nfuncs > MAX_FUNCS) {
        fprintf(stderr, "internal error: 函数数超过 %d\n", MAX_FUNCS);
        exit(2);
    }
    for (int j = 1; j < p->nfuncs; j++) {
        const IrFunc *f = &p->funcs[j];
        FuncInfo *inf = &g_info[j];
        inf->nnames = inf->nslots = 0;
        for (int i = 0; i < MAX_LABELS; i++) inf->idx_by_label[i] = -1;
        for (int k = 0; k < f->nparams; k++)
            slot_add(inf, f->params[k], 0);   /* 形参槽与实参拷贝对齐 */
        for (int i = 0; i < f->n; i++) {
            const Quad *q = &f->q[i];
            if (q->op == Q_LABEL && q->label < MAX_LABELS)
                inf->idx_by_label[q->label] = i;
            const char *cand[3] = { dest_of(q), NULL, q->z };
            if (q->op != Q_CALL) cand[1] = q->y;   /* CALL 的 y 是函数名！ */
            for (int c = 0; c < 3; c++) {
                const char *nm = cand[c];
                if (!nm || (nm[0] >= '0' && nm[0] <= '9')) continue; /* 空/立即数 */
                if (slot_find(m, nm) >= 0 || slot_find(inf, nm) >= 0) continue;
                const ArrayDesc *d = arr_of(p, nm);
                if (d) {               /* 数组基名：帧内占连续 len 格 */
                    if (inf->nslots + d->len > MAX_SLOTS ||
                        inf->nnames >= MAX_SLOTS) {
                        fprintf(stderr,
                            "internal error: 函数 %s 的帧容纳不下数组 %s\n",
                            f->name, d->name);
                        exit(2);
                    }
                    NameSlot *s = &inf->names[inf->nnames++];
                    s->name = d->name;
                    s->is_global = 0;
                    s->off  = inf->nslots;   /* 元素 i 在 off+i */
                    inf->nslots += d->len;
                    continue;
                }
                if (inf->nslots >= MAX_SLOTS) {
                    fprintf(stderr, "internal error: 函数 %s 局部名字超过 %d 个\n",
                            f->name, MAX_SLOTS);
                    exit(2);
                }
                slot_add(inf, nm, 0);
            }
        }
    }
}

/* ---------------- 2. 寻址：装载期的成果在这兑现 ---------------- */

static Val *slot_addr(int fi, const char *name)
{
    const FuncInfo *inf = &g_info[fi];
    int k = slot_find(inf, name);
    if (k < 0 && fi != 0) {
        /* 本帧没有 → 全局区（主段的名字表）。MiniC 的作用域规则在
         * 寻址上的样子：函数看得见全局、看不见别的函数的局部（7.3） */
        inf = &g_info[0];
        k = slot_find(inf, name);
    }
    if (k < 0) return NULL;                 /* 装载期保证不发生 */
    const NameSlot *s = &inf->names[k];
    return s->is_global ? &g_globals[s->off]
                        : &g_stack[fp + FRAME_HDR + s->off];
}

/* 取"地址"的值：数字开头 = 立即数文本（含 '.' 为 float），否则查槽。
 * 对比 irvm.c 同名函数：那里沿帧链找 Entry，这里一次下标。 */
static Val fetch(int fi, const char *s)
{
    Val v = {0, 0, 0};
    if (s[0] == '-' || (s[0] >= '0' && s[0] <= '9')) {
        /* 立即数 = 可选负号 + 数字（负号来自优化器折叠的常量文本）；
         * 含 '.'/'e'/'E' 为 float（%g 会把小值打成 1e-05） */
        int neg = s[0] == '-';
        const char *t = neg ? s + 1 : s;
        if (t[0] >= '0' && t[0] <= '9') {
            if (strpbrk(t, ".eE")) { v.isfloat = 1; v.d = strtod(t, NULL); }
            else                    v.i = strtol(t, NULL, 10);
            if (neg) { v.i = -v.i; v.d = -v.d; }
            return v;
        }
    }
    Val *a = slot_addr(fi, s);
    return a ? *a : v;
}

static void store(int fi, const char *name, Val v)
{
    *slot_addr(fi, name) = v;
}

/* ---------------- 3. 值运算（与 eval.c/irvm.c 逐字一致） ---------------- */

static Val arith2(TokenType op, Val a, Val b, int line)
{
    Val r = {0, 0, 0};
    if (a.isfloat || b.isfloat) {
        double x = a.isfloat ? a.d : (double)a.i;
        double y = b.isfloat ? b.d : (double)b.i;
        r.isfloat = 1;
        switch (op) {
        case T_PLUS:  r.d = x + y; break;
        case T_MINUS: r.d = x - y; break;
        case T_STAR:  r.d = x * y; break;
        case T_SLASH: r.d = x / y; break;
        default:      break;
        }
    } else {
        if ((op == T_SLASH || op == T_PERCENT) && b.i == 0) {
            fprintf(stderr, "line %d: runtime error: 整数%s除数为零\n",
                    line, op == T_SLASH ? "除法" : "取模");
            exit(1);
        }
        switch (op) {
        case T_PLUS:    r.i = a.i + b.i; break;
        case T_MINUS:   r.i = a.i - b.i; break;
        case T_STAR:    r.i = a.i * b.i; break;
        case T_SLASH:   r.i = a.i / b.i; break;
        case T_PERCENT: r.i = a.i % b.i; break;
        default:        break;
        }
    }
    return r;
}

/* 取跳转目标：装载期只为 <MAX_LABELS 的标号建表，读侧对称设防——
 * 越界 label 直接读 idx_by_label 是数组越界。 */
static int jump_target(const FuncInfo *fi, const Quad *q)
{
    if (q->label >= 0 && q->label < MAX_LABELS &&
        fi->idx_by_label[q->label] >= 0)
        return fi->idx_by_label[q->label];
    fprintf(stderr, "runtime error: 非法跳转标号 %d\n", q->label);
    exit(2);
}

static int compare(TokenType op, Val a, Val b)   /* 结果 int 0/1 */
{
    /* 整型同类型按 long 精确比较——MiniC int 即 32 位，语义上就该
     * 按整型比（经 double 中转虽在 |v|<2^53 内无损，但与折叠器、
     * 原生 cmpq 三方口径统一成"整型整比"最不易漂移）。 */
    if (!a.isfloat && !b.isfloat) {
        switch (op) {
        case T_LT: return a.i <  b.i;
        case T_LE: return a.i <= b.i;
        case T_GT: return a.i >  b.i;
        case T_GE: return a.i >= b.i;
        case T_EQ: return a.i == b.i;
        case T_NEQ: return a.i != b.i;
        default:   return 0;
        }
    }
    double x = a.isfloat ? a.d : (double)a.i;
    double y = b.isfloat ? b.d : (double)b.i;
    switch (op) {
    case T_LT: return x <  y;
    case T_LE: return x <= y;
    case T_GT: return x >  y;
    case T_GE: return x >= y;
    case T_EQ: return x == y;
    case T_NEQ: return x != y;
    default:   return 0;
    }
}

static int truthy(Val v) { return v.isfloat ? v.d != 0 : v.i != 0; }

/* 输出格式与 irvm.c 的 Q_PRINT 一致：%ld / %g */
static void fprint_val(FILE *out, Val v)
{
    if (v.isfloat) fprintf(out, "%g", v.d);
    else           fprintf(out, "%ld", v.i);
}

/* ---------------- 4. 栈操作 ---------------- */

static void push_val(Val v, int line)
{
    if (sp >= STACK_MAX) {
        fprintf(stderr, "line %d: runtime error: 栈溢出（递归太深？）\n", line);
        exit(1);
    }
    g_stack[sp++] = v;
}

/* ---------------- 5. 执行：单循环 + 显式机器状态，无 C 递归 ---------------- */

static int find_func_idx(const IrProgram *p, const char *name)
{
    for (int i = 0; i < p->nfuncs; i++)
        if (strcmp(p->funcs[i].name, name) == 0) return i;
    return -1;   /* 编译期已保证被调函数已定义（防御） */
}

int vm_run(const IrProgram *p, int trace)
{
    g_trace = trace;
    memset(g_globals, 0, sizeof g_globals);
    memset(g_stack, 0, sizeof g_stack);
    sp = fp = g_depth = 0;

    load_program(p);

    /* 引导帧：手铺一层哨兵帧当"系统调用者"——真实机器上是 crt0/_start
     * 干的活。主段末尾的隐式 return 会退到这里，段号 -1 = 停机。 */
    g_stack[0].i = -1;               /* 旧 fp 哨兵 */
    g_stack[1].i = -1;               /* 返回段号 -1：退到这里就停机 */
    g_stack[2].i = 0;                /* 返回 pc（不用） */
    sp = FRAME_HDR;
    fp = 0;

    int fi = 0;                      /* 当前段号：从主段开始 */
    int pc = 0;                      /* 当前指令下标 */

    for (;;) {
        const IrFunc *f = &p->funcs[fi];
        if (pc >= f->n) {            /* gen 保证每段以 return 结尾（防御） */
            fprintf(stderr, "internal error: 段 %s 未以 return 结束\n", f->name);
            exit(2);
        }
        const Quad *q = &f->q[pc];

        switch (q->op) {
        case Q_LABEL:
            break;                                   /* nop（装载期已建表） */
        case Q_GOTO:
            pc = jump_target(&g_info[fi], q);
            continue;
        case Q_IF_GOTO: {
            int c = compare(q->bop, fetch(fi, q->y), fetch(fi, q->z));
            if (c) { pc = jump_target(&g_info[fi], q); continue; }
            break;
        }
        case Q_IFF_GOTO:
            if (!truthy(fetch(fi, q->y))) {
                pc = jump_target(&g_info[fi], q);
                continue;
            }
            break;

        case Q_ASSIGN:
            store(fi, q->x, fetch(fi, q->y));
            break;
        case Q_BINOP: {
            /* 比较运算得 int 0/1（走 compare），算术走 arith2——
             * 分派方式与 irvm.c 一致 */
            TokenType bop = q->bop;
            int is_cmp = bop == T_LT || bop == T_LE || bop == T_GT ||
                         bop == T_GE || bop == T_EQ || bop == T_NEQ;
            Val a = fetch(fi, q->y), b = fetch(fi, q->z), r = {0, 0, 0};
            if (is_cmp) r.i = compare(bop, a, b);
            else        r = arith2(bop, a, b, q->line);
            store(fi, q->x, r);
            break;
        }
        case Q_NEG: {
            Val a = fetch(fi, q->y), r = {0, 0, 0};
            if (a.isfloat) { r.isfloat = 1; r.d = -a.d; }
            else             r.i = -a.i;
            store(fi, q->x, r);
            break;
        }
        case Q_NOT: {
            Val r = {0, 0, 0};
            r.i = !truthy(fetch(fi, q->y));
            store(fi, q->x, r);
            break;
        }
        case Q_I2F: {
            Val a = fetch(fi, q->y), r = {0, 0, 0};
            r.isfloat = 1;
            r.d = (double)a.i;
            store(fi, q->x, r);
            break;
        }

        case Q_LDX: {                         /* x = y[z]：数组读（6.4）。
                                               * 数组的 NameSlot.off 存的是
                                               * 连续块的基址槽，取首格地址
                                               * 再按整数下标偏移——"基址 +
                                               * 下标×宽度"在栈上的样子 */
            Val iv = fetch(fi, q->z), r = {0, 0, 0};
            Val *base = slot_addr(fi, q->y);
            if (base) r = base[iv.i];         /* 越界不查（同 C） */
            store(fi, q->x, r);
            break;
        }
        case Q_STX: {                         /* y[z] = x：数组写 */
            Val iv = fetch(fi, q->z), xv = fetch(fi, q->x);
            Val *base = slot_addr(fi, q->y);
            if (base) base[iv.i] = xv;
            break;
        }

        case Q_PARAM:                    /* 调用序列第一步：实参压栈 */
            push_val(fetch(fi, q->y), q->line);
            break;

        case Q_CALL: {
            int cf = find_func_idx(p, q->y);
            if (cf < 0) {
                fprintf(stderr, "internal error: 找不到函数 %s\n", q->y);
                exit(2);
            }
            int need = FRAME_HDR + g_info[cf].nslots;
            if (sp + need > STACK_MAX) { /* 提前检查，报错定位到 call */
                fprintf(stderr,
                        "line %d: runtime error: 栈溢出（递归太深？深度 %d）\n",
                        q->line, g_depth);
                exit(1);
            }

            /* --- 调用序列 caller 半程：压帧头，转跳 --- */
            Val t = {0, 0, 0};
            t.i = fp;     push_val(t, q->line);  /* 旧 fp（fp 链的钩子） */
            t.i = fi;     push_val(t, q->line);  /* 返回段号 */
            t.i = pc + 1; push_val(t, q->line);  /* 返回 pc：call 的下一格 */
            fp = sp - FRAME_HDR;                 /* fp 指向旧 fp 那一格 */
            g_depth++;

            /* 序言：实参从栈上的实参区拷进帧内形参槽（形参槽 0..n-1 恰
             * 与之对齐），其余局部清零——新帧读任何名字都是确定零值。
             * sp 越过整个局部区：这格空间从此归本帧所有，被调方自己再
             * 压栈（它的 param）只会落在更上面 */
            for (int k = 0; k < q->argc; k++)
                g_stack[fp + FRAME_HDR + k] =
                    g_stack[fp - q->argc + k];
            for (int k = FRAME_HDR + q->argc; k < FRAME_HDR + g_info[cf].nslots; k++)
                g_stack[fp + k] = (Val){0, 0, 0};
            sp = fp + FRAME_HDR + g_info[cf].nslots;

            if (g_trace) {
                fprintf(stderr, "[vm] call %-9s 深%-3d fp=%-5d sp=%-5d 实参(",
                        p->funcs[cf].name, g_depth, fp, sp);
                for (int k = 0; k < q->argc; k++) {
                    if (k) fputc(',', stderr);
                    fprintf(stderr, "%s=", p->funcs[cf].params[k]);
                    fprint_val(stderr, g_stack[fp + FRAME_HDR + k]);
                }
                fprintf(stderr, ") 局部%d格\n", g_info[cf].nslots - q->argc);
            }

            fi = cf;                         /* 跳到被调段开头（不 pc++） */
            pc = 0;
            continue;
        }

        case Q_RETURN: {
            /* --- 调用序列 callee 半程：放返回值，收帧，弹回 --- */
            if (q->y) {
                acc = fetch(fi, q->y);
            } else {                          /* 隐式 return：类型零值 */
                acc = (Val){0, 0, 0};
                acc.isfloat = (f->rettype == TY_FLOAT);
            }
            int old_fp = (int)g_stack[fp + 0].i;
            int ret_fi = (int)g_stack[fp + 1].i;
            int ret_pc = (int)g_stack[fp + 2].i;

            sp = fp;                          /* 收帧：丢掉局部+帧头 */
            fp = old_fp;                      /* 顺 fp 链回到调用者的帧 */
            g_depth--;

            if (ret_fi < 0) return 0;         /* 退到引导哨兵：整个程序结束 */

            /* cdecl：实参归 caller 清。回去落点的前一条恰是发起调用的
             * call（不变式：return 只落在 call+1），argc 就在那条里。 */
            const Quad *cs = &p->funcs[ret_fi].q[ret_pc - 1];
            if (cs->op != Q_CALL) {
                fprintf(stderr, "internal error: return 落点不是 call 之后\n");
                exit(2);
            }
            sp -= cs->argc;
            /* 返回值落位：acc 存进 caller 的结果临时（四元式没有
             * "取累加器"指令，绑定动作归入完成调用的最后一步）；
             * void 调用的 x 为 NULL，跳过 */
            if (cs->x) store(ret_fi, cs->x, acc);

            if (g_trace) {
                fprintf(stderr, "[vm] ret  %-9s 深%-3d -> ", f->name, g_depth);
                fprint_val(stderr, acc);
                fprintf(stderr, "  回到 %s:%d  sp=%d\n",
                        p->funcs[ret_fi].name, ret_pc, sp);
            }

            fi = ret_fi;                      /* 弹回调用者（不 pc++） */
            pc = ret_pc;
            continue;
        }

        case Q_PRINT: {
            Val v = fetch(fi, q->y);
            fprint_val(stdout, v);
            putchar('\n');
            break;
        }

        default:
            break;
        }
        pc++;
    }
}
