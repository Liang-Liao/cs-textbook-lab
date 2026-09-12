/*
 * irvm.c —— 四元式解释器：lab6 的默认执行引擎，lab7 的对照引擎
 *
 * （lab7 起 vm.c 用显式活动记录取代这里的 C 递归调用栈；本文件与
 *   各 lab 逐字同步——含数组语义。lab6 里它是默认执行引擎，其余
 *   lab 经 -irvm 开关启用：同一份 IR，对照"机器"。）
 *
 * 结构就是一个最朴素的小虚拟机：pc 从 0 走到 return。
 *   - 变量环境：按 **IR 名字** 找值（gen 已保证名字全局唯一——对照
 *     eval.c 按 Sym* 找值：同一门语言在两种表示下的寻址方式）；
 *   - 标号：每函数执行前把 Q_LABEL 预处理成"标号号 → 指令下标"表，
 *     跳转即 pc 赋值；
 *   - 调用：param 指令把实参压入参数栈，call 从栈顶取 argc 个按位
 *     绑定到形参，然后用 **C 递归**执行被调函数——C 的调用栈暂时
 *     兼任 MiniC 的调用栈。lab7 把这层"看不见的栈"显式化：活动
 *     记录、返回地址、真正的栈式 VM。
 *   - 数组（6.4）：Q_LDX/Q_STX 按基名+下标访问挂在帧上的连续格；
 *     存储按 IrProgram.arrays 描述符惰性分配——全局数组落全局帧，
 *     局部数组每次激活各开一份（递归安全）。
 *
 * 求值语义与 eval.c 逐字一致（int/int 保 int、任一 float 提升、
 * 比较得 int、print %ld/%g）——run_tests.sh 的"双实现对拍"验证。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "irvm.h"

/* ---------------- 运行时值（与 eval.c 一致） ---------------- */

typedef struct {
    int    isfloat;
    long   i;
    double d;
} Val;

/* ---------------- 环境：帧链（键是 IR 名字） ---------------- */

typedef struct Entry {
    const char *name;
    Val         val;
    struct Entry *next;
} Entry;

/* 数组存储（6.4）：挂在帧上、按名字查。惰性分配——第一次被 LDX/STX
 * 访问时才开 len 格；全局数组落进全局帧，局部数组进当前激活的帧，
 * 递归的每层各有一份。 */
typedef struct ArrEntry {
    const char *name;
    Val        *cells;
    int         len;
    struct ArrEntry *next;
} ArrEntry;

typedef struct Frame {
    struct Frame *parent;
    Entry *head;
    ArrEntry *arrays;
} Frame;

static Frame *g_frame;   /* 全局帧：所有函数帧的父（同 eval.c） */

static Frame *frame_push(Frame *parent)
{
    Frame *f = calloc(1, sizeof *f);
    f->parent = parent;
    return f;
}

static void frame_pop(Frame *f)
{
    Entry *e = f->head;
    while (e) { Entry *nx = e->next; free(e); e = nx; }
    ArrEntry *a = f->arrays;
    while (a) { ArrEntry *nx = a->next; free(a->cells); free(a); a = nx; }
    free(f);
}

static void frame_bind(Frame *env, const char *name, Val v)
{
    for (Frame *f = env; f; f = f->parent)
        for (Entry *e = f->head; e; e = e->next)
            if (strcmp(e->name, name) == 0) { e->val = v; return; }
    Entry *e = calloc(1, sizeof *e);   /* 没绑过：新建 */
    e->name = name;
    e->val  = v;
    e->next = env->head;
    env->head = e;
}

/* 取"地址"的值：立即数（数字开头；含 '.'/'e'/'E' 为 float——%g 会把
 * 0.00001 打成 1e-05，只查 '.' 会误判成 int）或沿帧链找变量 */
static Val fetch(Frame *env, const char *s)
{
    Val v = {0, 0, 0};
    int neg_ = s[0] == '-';
    const char *t_ = neg_ ? s + 1 : s;
    if (neg_ || (t_[0] >= '0' && t_[0] <= '9')) {
        if (strpbrk(t_, ".eE")) { v.isfloat = 1; v.d = strtod(t_, NULL); }
        else                    v.i = strtol(t_, NULL, 10);
        if (neg_) { v.i = -v.i; v.d = -v.d; }
        return v;
    }
    for (Frame *f = env; f; f = f->parent)
        for (Entry *e = f->head; e; e = e->next)
            if (strcmp(e->name, s) == 0) return e->val;
    return v;   /* 未绑定的名字：编译期保证不会发生（decl 生成 x=0） */
}

/* 找数组（沿帧链）；没有就按 IrProgram.arrays 描述符惰性分配：
 * is_global 的落全局帧，否则进当前激活的帧（递归各层独立） */
static ArrEntry *frame_get_arr(Frame *env, const IrProgram *p,
                               const char *name)
{
    for (Frame *f = env; f; f = f->parent)
        for (ArrEntry *a = f->arrays; a; a = a->next)
            if (strcmp(a->name, name) == 0) return a;

    const ArrayDesc *d = NULL;
    for (int i = 0; i < p->narrays; i++)
        if (strcmp(p->arrays[i].name, name) == 0) { d = &p->arrays[i]; break; }
    if (!d) {
        fprintf(stderr, "internal error: 数组 %s 缺少描述符\n", name);
        exit(2);
    }
    Frame *home   = d->is_global ? g_frame : env;
    ArrEntry *a   = calloc(1, sizeof *a);
    a->name       = d->name;
    a->len        = d->len;
    a->cells      = calloc((size_t)d->len, sizeof(Val));   /* 零初始化 */
    a->next       = home->arrays;
    home->arrays  = a;
    return a;
}

/* ---------------- 值运算（与 eval.c 逐字一致） ---------------- */

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

static int compare(TokenType op, Val a, Val b)   /* 结果 int 0/1 */
{
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

/* ---------------- 执行 ---------------- */

#define MAX_LABELS 4096

static Val param_stack[256];   /* param 指令压栈；call 弹 argc 个 */
static int  ptop;

static const IrFunc *find_func(const IrProgram *p, const char *name)
{
    for (int i = 0; i < p->nfuncs; i++)
        if (strcmp(p->funcs[i].name, name) == 0) return &p->funcs[i];
    return NULL;   /* 编译期已保证被调函数已定义（防御） */
}

/* 取跳转目标：写表侧只登记 <MAX_LABELS 的标号，读侧对称设防——
 * 越界 label 直接读 idx_by_label 是数组越界。 */
static int jump_target(const int *idx_by_label, const Quad *q)
{
    if (q->label >= 0 && q->label < MAX_LABELS &&
        idx_by_label[q->label] >= 0)
        return idx_by_label[q->label];
    fprintf(stderr, "runtime error: 非法跳转标号 %d\n", q->label);
    exit(2);
}

/* 执行一个函数段：args 指向实参数组（与 params[] 同序）。
 * env 由调用者提供并管理生命周期——主段直接用全局帧（顶层声明的
 * 变量就是全局变量！），函数调用用 frame_push(g_frame) 新帧。 */
static Val run_func(const IrProgram *p, const IrFunc *f,
                    const Val *args, int nargs, Frame *env)
{
    /* 标号号 → 指令下标（Q_LABEL 在执行时是 nop，这里先建跳转表）。
     * 读侧与写侧对称设防：越界/未登记的 label 报错而非数组越界读。 */
    int idx_by_label[MAX_LABELS];
    for (int i = 0; i < MAX_LABELS; i++) idx_by_label[i] = -1;
    for (int i = 0; i < f->n; i++)
        if (f->q[i].op == Q_LABEL && f->q[i].label >= 0 &&
            f->q[i].label < MAX_LABELS)
            idx_by_label[f->q[i].label] = i;

    /* 形参绑定到调用者提供的帧（主段 = 全局帧；函数段 = 调用处新建的
     * g_frame 子帧——帧的生命周期由调用者管理，本函数不 pop） */
    for (int i = 0; i < f->nparams; i++)
        frame_bind(env, f->params[i], i < nargs ? args[i] : (Val){0});

    Val ret = {0, 0, 0};
    if (f->rettype == TY_FLOAT) ret.isfloat = 1;   /* 隐式 return 的零值 */

    int pc = 0;
    while (pc < f->n) {
        const Quad *q = &f->q[pc];
        switch (q->op) {
        case Q_LABEL:
            break;                                  /* nop（已在跳转表里） */
        case Q_GOTO:
            pc = jump_target(idx_by_label, q);
            continue;
        case Q_IF_GOTO: {
            int c = compare(q->bop, fetch(env, q->y), fetch(env, q->z));
            if (c) { pc = jump_target(idx_by_label, q); continue; }
            break;
        }
        case Q_IFF_GOTO:
            if (!truthy(fetch(env, q->y))) { pc = jump_target(idx_by_label, q); continue; }
            break;
        case Q_ASSIGN:
            frame_bind(env, q->x, fetch(env, q->y));
            break;
        case Q_BINOP: {
            /* 比较运算得 int 0/1（走 compare），算术走 arith2——
             * 与 eval.c 的分派完全一致 */
            TokenType bop = q->bop;
            int is_cmp = bop == T_LT || bop == T_LE || bop == T_GT ||
                         bop == T_GE || bop == T_EQ || bop == T_NEQ;
            Val a = fetch(env, q->y), b = fetch(env, q->z), r = {0, 0, 0};
            if (is_cmp) r.i = compare(bop, a, b);
            else        r = arith2(bop, a, b, q->line);
            frame_bind(env, q->x, r);
            break;
        }
        case Q_NEG: {
            Val a = fetch(env, q->y), r = {0, 0, 0};
            if (a.isfloat) { r.isfloat = 1; r.d = -a.d; }
            else             r.i = -a.i;
            frame_bind(env, q->x, r);
            break;
        }
        case Q_NOT: {
            Val r = {0, 0, 0};
            r.i = !truthy(fetch(env, q->y));
            frame_bind(env, q->x, r);
            break;
        }
        case Q_I2F: {
            Val a = fetch(env, q->y), r = {0, 0, 0};
            r.isfloat = 1;
            r.d = (double)a.i;
            frame_bind(env, q->x, r);
            break;
        }
        case Q_LDX: {                         /* x = y[z]（6.4 数组读） */
            ArrEntry *a = frame_get_arr(env, p, q->y);
            Val iv = fetch(env, q->z), r = {0, 0, 0};
            if (a) r = a->cells[iv.i];        /* 越界不查（同 C） */
            frame_bind(env, q->x, r);
            break;
        }
        case Q_STX: {                         /* y[z] = x（6.4 数组写） */
            ArrEntry *a = frame_get_arr(env, p, q->y);
            Val iv = fetch(env, q->z);
            Val xv = fetch(env, q->x);
            if (a) a->cells[iv.i] = xv;
            break;
        }
        case Q_PARAM:
            param_stack[ptop++] = fetch(env, q->y);
            break;
        case Q_CALL: {
            const IrFunc *callee = find_func(p, q->y);
            if (!callee) {
                fprintf(stderr, "internal error: 找不到函数 %s\n", q->y);
                exit(2);
            }
            ptop -= q->argc;                        /* 弹 argc 个实参 */
            /* 被调函数的新帧：父 = 全局帧（MiniC 无嵌套函数，函数里
             * 看得见全局变量、看不见调用方的局部——与 eval.c 一致） */
            Frame *callee_env = frame_push(g_frame);
            Val rv = run_func(p, callee,
                              &param_stack[ptop], q->argc, callee_env);
            frame_pop(callee_env);
            if (q->x) frame_bind(env, q->x, rv);
            break;
        }
        case Q_RETURN:
            if (q->y) ret = fetch(env, q->y);
            return ret;
        case Q_PRINT: {
            Val v = fetch(env, q->y);
            if (v.isfloat) printf("%g\n", v.d);
            else           printf("%ld\n", v.i);
            break;
        }
        default:
            break;
        }
        pc++;
    }
    return ret;   /* 落到段尾的隐式 return（gen 已保证段尾有 return，防御） */
}

int irvm_run(const IrProgram *p)
{
    ptop = 0;
    g_frame = frame_push(NULL);
    /* 主段直接跑在全局帧上：顶层声明的变量就是全局变量，函数里
     * `g = g + 1` 改的正是这一份（此前主段用了私有帧，副作用全丢——
     * 双实现对拍抓出来的第二个 bug） */
    (void)run_func(p, &p->funcs[0], NULL, 0, g_frame);
    frame_pop(g_frame);
    g_frame = NULL;
    return 0;
}
