/*
 * codegen.c —— 四元式 → x86-64 目标码（本 lab 主角；对照龙书 8.1~8.3）
 *
 * 【目标机】x86-64，Windows 调用约定，AT&T 汇编语法——本仓库按
 * MSYS2/MinGW gcc 验证：`gcc prog.s src/rt.c -o prog.exe` 一条命令
 * 汇编链接。同一份 IR，lab7 的 VM 用"虚拟栈"解释它，这里把它落成
 * 真机器指令——对比着读两个后端，是本套实验的主线体验。
 *
 * 【降低策略】龙书 8.2 的"逐四元式翻译 + 最朴素 getReg"：每个 IR 名
 * 占帧内一个 8 字节槽（数组占连续 len 格），每条指令把操作数从内存
 * 装进寄存器、算完写回。没有寄存器分配——所有值都过内存，慢但正确
 * 且极易读懂；把"值留在寄存器里"正是第 8 章后半与真实编译器的全部
 * 复杂度所在（练习）。只用 caller-saved 寄存器（rax/rcx/rdx/xmm0-5），
 * 跨调用无须保存恢复。
 *
 * 【栈帧布局】（Windows x64；call 时要求 rsp 16 字节对齐）
 *
 *      rbp+48+8k   第 5+k 个实参（调用者写在它的出栈区里）
 *      rbp+16..47  影子空间（我们不用，但必须给被调方预留）
 *      rbp+8       返回地址          ← call 指令压入
 *      rbp+0       旧 rbp            ← pushq %rbp
 *      rbp-8(k+1)  局部槽 k（标量名/数组块基址），装载期定死
 *      ...         实参暂存槽 max_argc 格（param 先暂存、call 再装寄存器）
 *      rsp..       出栈区：32 字节影子空间 + 第 5 个起的实参格
 *      帧总大小 F ≡ 0 (mod 16)
 *
 * 【调用序列】（caller/callee 分工，7.2 的机器版）
 *   caller: param ×n —— 每个实参求值后暂存进本帧的暂存槽；
 *           call     —— 前 4 个按类型装 rcx/rdx/r8/r9 或 xmm0-3，
 *                       第 5 个起搬进出栈区，call 转移；
 *   callee: 序言压帧 + 把形参从寄存器/栈上搬进自己的槽位；
 *           return   —— 返回值放 rax（int）/xmm0（float），收帧 ret；
 *   caller: 结果临时从 rax/xmm0 取回。出栈区属于本帧，无需清理——
 *           这是固定帧的好处（7.2 "谁清实参"之争在这里消失）。
 *
 * 【全局与数组】funcs[0] 里被赋值的名字 = 全局变量 → .comm 标签零
 * 初始化；is_global 数组同理（len*8 字节）。函数局部数组在帧里占
 * 连续格，a[i] 就是 leaq 基址 + [基址+i*8]——x86 变址寻址的主场。
 * 局部区在序言里 rep stosq 清零，"声明即零初始化"由此兑现。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

#define MAX_NAMES 512
#define MAX_GLOBS 1024
#define MAX_LITS  256

static const IrProgram *P;
static FILE *OUT;

/* ---------------- 文件级符号表 ---------------- */

/* 全局名字集：funcs[0] 中被赋值的名字 + is_global 的数组基名 */
static struct { const char *name; int bytes; } GLOB[MAX_GLOBS];
static int nglob;

/* 浮点字面量池（去重）：操作数文本 → .rdata 里的 .double 标签 */
static struct { double val; } LIT[MAX_LITS];
static int nlit;

static int is_glob(const char *s)
{
    for (int i = 0; i < nglob; i++)
        if (strcmp(GLOB[i].name, s) == 0) return 1;
    return 0;
}

static void add_glob(const char *s, int bytes)
{
    if (nglob >= MAX_GLOBS) return;
    for (int i = 0; i < nglob; i++)
        if (strcmp(GLOB[i].name, s) == 0) {
            if (bytes > GLOB[i].bytes) GLOB[i].bytes = bytes;
            return;
        }
    GLOB[nglob].name  = s;
    GLOB[nglob].bytes = bytes;
    nglob++;
}

/* 浮点立即数登记进字面量池（标签 .LITn 在文件尾部统一输出）。
 * 池满必须 fail-fast：返回 nlit++ 却不登记条目，会生成引用未输出
 * 的 .LITn 标签的汇编（汇编期才炸，更难定位）。 */
static int lit_of(const char *text)
{
    double v = strtod(text, NULL);
    for (int i = 0; i < nlit; i++)
        if (LIT[i].val == v) return i;
    if (nlit >= MAX_LITS) {
        fprintf(stderr, "error: 浮点字面量池超过上限 %d\n", MAX_LITS);
        exit(2);
    }
    LIT[nlit].val = v;
    return nlit++;
}

/* ---------------- 操作数的静态分类 ---------------- */

/* 立即数识别：可选负号 + 数字开头（负号来自优化器折叠的常量文本；
 * lab8 自身无 -O 不会产生，但对齐 lab9 的 imm_num 口径防患未然） */
static int imm_int(const char *s)
{
    if (s[0] == '-') s++;
    return s[0] >= '0' && s[0] <= '9';
}
static int imm_float(const char *s)
{
    /* %g 会把小值打成 1e-05（有 e 无点），所以两类记号都要认 */
    return imm_int(s) && strpbrk(s, ".eE") != NULL;
}

/* 名字的静态类型：types[] 平表查得到就信它；立即数看记法；默认 int */
static Type type_of(const char *s)
{
    if (imm_int(s)) return imm_float(s) ? TY_FLOAT : TY_INT;
    for (int i = 0; i < P->ntypes; i++)
        if (strcmp(P->types[i].name, s) == 0) return P->types[i].ty;
    return TY_INT;
}

/* 数组描述符查找（同 vm.c） */
static const ArrayDesc *arr_of(const char *name)
{
    for (int i = 0; i < P->narrays; i++)
        if (strcmp(P->arrays[i].name, name) == 0) return &P->arrays[i];
    return NULL;
}

static int find_fn(const char *name)
{
    for (int i = 0; i < P->nfuncs; i++)
        if (strcmp(P->funcs[i].name, name) == 0) return i;
    return -1;
}

/* ---------------- 段内的装载期信息（与 vm.c 同一套思想） ---------------- */

typedef struct {
    const char *name;
    int         slot;    /* 槽序：字节地址 = -8*(slot+1)(%rbp)；
                            数组名记的是连续块的基址槽序 */
} NSlot;

static NSlot LOC[MAX_NAMES];
static int   nloc;           /* 本段的名字条数 */
static int   nslot_total;    /* 名字占掉的槽总数（数组展开计 len 格） */
static int   max_argc;       /* 单次调用最大实参数：决定出栈区大小 */
static int   stg_depth;      /* 实参暂存栈的当前深度（param/call 配对） */
static int   stg_peak;       /* 深度峰值：决定暂存槽总数——嵌套调用的
                              * 外层实参会先入暂存，内层压在它上面 */
static int   g_cur_fi;       /* 当前段的序号（报错定位用） */

static int find_local(const char *name)
{
    for (int k = 0; k < nloc; k++)
        if (strcmp(LOC[k].name, name) == 0) return k;
    return -1;
}

static int byte_off(int slot) { return -8 * (slot + 1); }

/* 这些 op 的 x 会写入一个名字（Q_CALL 的 x 可为 NULL） */
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

/* 登记一个名字：数组占连续块，标量占一格。全局名字不进局部表。 */
static void add_local(const char *nm)
{
    const ArrayDesc *d = arr_of(nm);
    int len = (d && !d->is_global) ? d->len : 1;
    if (d && d->is_global) return;              /* 全局数组走 .comm 标签 */
    if (nloc >= MAX_NAMES || nslot_total + len > MAX_NAMES) {
        fprintf(stderr, "internal error: 段 %s 局部存储超限\n",
                P->funcs[g_cur_fi].name);
        exit(2);
    }
    LOC[nloc].name = nm;
    LOC[nloc].slot = nslot_total;
    nloc++;
    nslot_total += len;
}

/* 扫描一段：形参先占槽，再扫指令登记目标/数组基名，统计最大 argc
 * 与暂存深度峰值。与 vm.c 的 load_program 逐行对应——两个后端共享
 * "装载期定槽位"。 */
static void scan_segment(int fi)
{
    const IrFunc *f = &P->funcs[fi];
    g_cur_fi = fi;
    nloc = nslot_total = max_argc = stg_depth = stg_peak = 0;

    for (int j = 0; j < f->nparams; j++) add_local(f->params[j]);
    for (int i = 0; i < f->n; i++) {
        const Quad *q = &f->q[i];
        if (q->op == Q_CALL) {
            if (q->argc > max_argc) max_argc = q->argc;
            stg_depth -= q->argc;
        } else if (q->op == Q_PARAM) {
            if (++stg_depth > stg_peak) stg_peak = stg_depth;
        }

        const char *dst = dest_of(q);
        const char *y   = (q->op != Q_CALL) ? q->y : NULL;  /* CALL 的 y 是函数名 */
        const char *cand[3] = { dst, y, q->z };
        for (int c = 0; c < 3; c++) {
            const char *nm = cand[c];
            if (!nm || imm_int(nm)) continue;
            if (is_glob(nm) || find_local(nm) >= 0) continue;
            add_local(nm);
        }
    }
}

/* ---------------- 发指令的小工具 ---------------- */

/* '@' 是 IR 名的合法字符但不是标签的——mangle 成 '_'（x@1→x_1） */
static void mangle(char *dst, size_t cap, const char *s)
{
    size_t w = 0;
    for (; *s && w + 1 < cap; s++) dst[w++] = (*s == '@') ? '_' : *s;
    dst[w] = '\0';
}

/* 名字的内存操作数写到 buf："-24(%rbp)"（局部）或 "G_x(%rip)"（全局）。
 * 返回 0=局部 1=全局；未登记返回 -1（防御）。
 * 注意 byte_off 本身返回负值，这里不再补负号。 */
static int addr_str(const char *name, char *buf, size_t cap)
{
    int k = find_local(name);
    if (k >= 0) {
        snprintf(buf, cap, "%d(%%rbp)", byte_off(LOC[k].slot));
        return 0;
    }
    if (is_glob(name)) {
        char m[128];
        mangle(m, sizeof m, name);
        snprintf(buf, cap, "G_%s(%%rip)", m);
        return 1;
    }
    return -1;
}

/* 整数值装入任意通用寄存器（立即数 / 全局 / 局部一视同仁） */
static void load_int(const char *s, const char *reg)
{
    if (imm_int(s)) { fprintf(OUT, "\tmovq $%s, %s\n", s, reg); return; }
    char a[160];
    int k = addr_str(s, a, sizeof a);
    if (k < 0) { fprintf(OUT, "\txorl %%eax, %%eax\n"); return; }  /* 防御 */
    fprintf(OUT, "\tmovq %s, %s\n", a, reg);
}

/* 浮点值装入 xmm 寄存器（立即数走字面量池） */
static void load_float(const char *s, const char *xmm)
{
    if (imm_float(s)) {
        fprintf(OUT, "\tmovsd .LIT%d(%%rip), %s\n", lit_of(s), xmm);
        return;
    }
    char a[160];
    int k = addr_str(s, a, sizeof a);
    if (k < 0) { fprintf(OUT, "\txorpd %s, %s\n", xmm, xmm); return; }
    fprintf(OUT, "\tmovsd %s, %s\n", a, xmm);
}

/* rax / xmm0 写回名字（局部走 rbp 相对，全局走 rip 相对标签） */
static void store_int(const char *name)
{
    char a[160];
    if (addr_str(name, a, sizeof a) >= 0)
        fprintf(OUT, "\tmovq %%rax, %s\n", a);
}
static void store_float(const char *name)
{
    char a[160];
    if (addr_str(name, a, sizeof a) >= 0)
        fprintf(OUT, "\tmovsd %%xmm0, %s\n", a);
}

static int seqno;                               /* 局部标签流水号 */

/* 除零检查：除数在 rcx；为零则带源行号进运行时助手（不返回） */
static void emit_div_check(TokenType op, int line)
{
    int n = seqno++;
    fprintf(OUT, "\ttestq %%rcx, %%rcx\n");
    fprintf(OUT, "\tjnz .LS%d\n", n);
    fprintf(OUT, "\tmovl $%d, %%ecx\n", line);
    fprintf(OUT, "\tcall %s\n", op == T_SLASH ? "minic_div_zero"
                                              : "minic_mod_zero");
    fprintf(OUT, ".LS%d:\n", n);
}

/* 比较运算的 setcc/jcc（整数有符号 / 浮点无序忽略，见 README 说明） */
static const char *setcc_int(TokenType op)
{
    switch (op) {
    case T_LT: return "setl";  case T_LE: return "setle";
    case T_GT: return "setg";  case T_GE: return "setge";
    case T_EQ: return "sete";  default:  return "setne";
    }
}
static const char *setcc_flt(TokenType op)
{
    switch (op) {
    case T_LT: return "setb";  case T_LE: return "setbe";
    case T_GT: return "seta";  case T_GE: return "setae";
    case T_EQ: return "sete";  default:  return "setne";
    }
}
static const char *jcc_int(TokenType op)
{
    switch (op) {
    case T_LT: return "jl";    case T_LE: return "jle";
    case T_GT: return "jg";    case T_GE: return "jge";
    case T_EQ: return "je";    default:  return "jne";
    }
}
/* 【已知限制】浮点 NaN 比较的无序语义：jb/jbe 对 NaN 为真、je 把 NaN
 * 当假；解释器 truthy(NaN)=真——两边相反且都不合 IEEE"无序=假"。
 * MiniC 教学子集不产生 NaN，分歧仅在不可达场景；记录在案，不为它
 * 引入 comisd+jp 的双分支复杂度。（词法层接受 1e999 一类字面量会得到
 * inf 而非 NaN，属另一条已知边角，见 README"已知限制"。） */
static const char *jcc_flt(TokenType op)
{
    switch (op) {
    case T_LT: return "jb";    case T_LE: return "jbe";
    case T_GT: return "ja";    case T_GE: return "jae";
    case T_EQ: return "je";    default:  return "jne";
    }
}

/* ---------------- 段的输出 ---------------- */

static const char *ARGREG[4] = { "%rcx", "%rdx", "%r8", "%r9" };

static void emit_function(int fi)
{
    scan_segment(fi);
    const IrFunc *f = &P->funcs[fi];
    char label[160], m[128];

    if (fi == 0) strcpy(label, "mc_top");
    else { mangle(m, sizeof m, f->name); snprintf(label, sizeof label,
                                                  "fn_%s", m); }

    /* ---- 帧大小：命名槽 + 实参暂存槽（按嵌套深度峰值） + 出栈区
     * （影子空间必留 32B），向上取整到 16 的倍数——保证每次 call
     * 时 rsp 16 字节对齐 ---- */
    int staging = stg_peak;
    int oa = 32 + 8 * (max_argc > 4 ? max_argc - 4 : 0);
    int F = ((8 * (nslot_total + staging) + oa) + 15) & ~15;

    fprintf(OUT, "\t.globl %s\n", label);
    fprintf(OUT, "%s:\n", label);
    fprintf(OUT, "\tpushq %%rbp\n");
    fprintf(OUT, "\tmovq %%rsp, %%rbp\n");
    if (F) fprintf(OUT, "\tsubq $%d, %%rsp\n", F);

    /* 形参搬进槽位：前 4 个来自寄存器（整 rcx/rdx/r8/r9，浮 xmm0-3），
     * 第 5 个起本来就在栈上 [rbp+48+8k]，搬进自己槽位统一寻址。
     * 必须先于清零循环——rep stosq 会破坏 rcx/rdx！ */
    for (int j = 0; j < f->nparams; j++) {
        int off = byte_off(j);
        int flt = type_of(f->params[j]) == TY_FLOAT;
        if (j < 4) {
            if (flt) fprintf(OUT, "\tmovsd %%xmm%d, %d(%%rbp)\n", j, off);
            else     fprintf(OUT, "\tmovq %s, %d(%%rbp)\n", ARGREG[j], off);
        } else {
            int src = 48 + 8 * (j - 4);
            if (flt) fprintf(OUT, "\tmovsd %d(%%rbp), %%xmm0\n"
                                 "\tmovsd %%xmm0, %d(%%rbp)\n", src, off);
            else     fprintf(OUT, "\tmovq %d(%%rbp), %%rax\n"
                                 "\tmovq %%rax, %d(%%rbp)\n", src, off);
        }
    }

    /* 序言清零"非形参"的局部区（含数组块与暂存槽）——"声明即零初始化"
     * 不变量的机器版。形参槽刚拷好，跳过它们；寄存器此时已可牺牲 */
    {
        int tail_q = nslot_total + staging - f->nparams;
        if (tail_q > 0) {
            fprintf(OUT, "\tleaq %d(%%rbp), %%rdi\n",
                    -8 * (f->nparams + tail_q));
            fprintf(OUT, "\tmovq $%d, %%rcx\n", tail_q);
            fprintf(OUT, "\txorl %%eax, %%eax\n");
            fprintf(OUT, "\trep stosq\n");
        }
    }

    /* ---- 逐条翻译四元式 ---- */
    for (int i = 0; i < f->n; i++) {
        const Quad *q = &f->q[i];
        switch (q->op) {
        case Q_LABEL:
            fprintf(OUT, ".L%d:\n", q->label);
            break;
        case Q_GOTO:
            fprintf(OUT, "\tjmp .L%d\n", q->label);
            break;
        case Q_IF_GOTO: {
            int flt = type_of(q->y) == TY_FLOAT || type_of(q->z) == TY_FLOAT;
            if (flt) {
                load_float(q->y, "%xmm0");
                load_float(q->z, "%xmm1");
                fprintf(OUT, "\tcomisd %%xmm1, %%xmm0\n");
                fprintf(OUT, "\t%s .L%d\n", jcc_flt(q->bop), q->label);
            } else {
                load_int(q->y, "%rax");
                load_int(q->z, "%rcx");
                fprintf(OUT, "\tcmpq %%rcx, %%rax\n");
                fprintf(OUT, "\t%s .L%d\n", jcc_int(q->bop), q->label);
            }
            break;
        }
        case Q_IFF_GOTO: {                    /* 数值真值：为零跳转 */
            if (type_of(q->y) == TY_FLOAT) {
                load_float(q->y, "%xmm0");
                fprintf(OUT, "\txorpd %%xmm1, %%xmm1\n");
                fprintf(OUT, "\tcomisd %%xmm1, %%xmm0\n");
            } else {
                load_int(q->y, "%rax");
                fprintf(OUT, "\ttestq %%rax, %%rax\n");
            }
            fprintf(OUT, "\tje .L%d\n", q->label);
            break;
        }

        case Q_ASSIGN: {
            if (type_of(q->x) == TY_FLOAT) {
                load_float(q->y, "%xmm0"); store_float(q->x);
            } else {
                load_int(q->y, "%rax"); store_int(q->x);
            }
            break;
        }
        case Q_BINOP: {
            int is_cmp = q->bop == T_LT || q->bop == T_LE ||
                         q->bop == T_GT || q->bop == T_GE ||
                         q->bop == T_EQ || q->bop == T_NEQ;
            int flt = !is_cmp && type_of(q->x) == TY_FLOAT;
            if (is_cmp) {
                /* 比较得 int 0/1；浮点比较用 comisd + 无序变体 */
                if (type_of(q->y) == TY_FLOAT || type_of(q->z) == TY_FLOAT) {
                    load_float(q->y, "%xmm0");
                    load_float(q->z, "%xmm1");
                    fprintf(OUT, "\tcomisd %%xmm1, %%xmm0\n");
                    fprintf(OUT, "\t%s %%al\n", setcc_flt(q->bop));
                } else {
                    load_int(q->y, "%rax");
                    load_int(q->z, "%rcx");
                    fprintf(OUT, "\tcmpq %%rcx, %%rax\n");
                    fprintf(OUT, "\t%s %%al\n", setcc_int(q->bop));
                }
                fprintf(OUT, "\tmovzbq %%al, %%rax\n");
            } else if (flt) {
                load_float(q->y, "%xmm0");
                load_float(q->z, "%xmm1");
                switch (q->bop) {
                case T_PLUS:  fprintf(OUT, "\taddsd %%xmm1, %%xmm0\n"); break;
                case T_MINUS: fprintf(OUT, "\tsubsd %%xmm1, %%xmm0\n"); break;
                case T_STAR:  fprintf(OUT, "\tmulsd %%xmm1, %%xmm0\n"); break;
                case T_SLASH: fprintf(OUT, "\tdivsd %%xmm1, %%xmm0\n"); break;
                default: break;               /* 浮点取模已被前端拒绝 */
                }
            } else if (q->bop == T_SLASH || q->bop == T_PERCENT) {
                /* 整数除/模：先查除零（带行号进运行时助手），cqto+idivq：
                 * 商在 rax、余数在 rdx——MiniC 的 % 取的就是余数 */
                load_int(q->z, "%rcx");
                emit_div_check(q->bop, q->line);
                load_int(q->y, "%rax");
                fprintf(OUT, "\tcqto\n");
                fprintf(OUT, "\tidivq %%rcx\n");
                if (q->bop == T_PERCENT)
                    fprintf(OUT, "\tmovq %%rdx, %%rax\n");
            } else {
                load_int(q->y, "%rax");
                load_int(q->z, "%rcx");
                switch (q->bop) {
                case T_PLUS:  fprintf(OUT, "\taddq %%rcx, %%rax\n"); break;
                case T_MINUS: fprintf(OUT, "\tsubq %%rcx, %%rax\n"); break;
                case T_STAR:  fprintf(OUT, "\timulq %%rcx, %%rax\n"); break;
                default: break;
                }
            }
            if (is_cmp || type_of(q->x) == TY_INT) store_int(q->x);
            else store_float(q->x);
            break;
        }
        case Q_NEG: {
            if (type_of(q->y) == TY_FLOAT) {
                load_float(q->y, "%xmm0");
                fprintf(OUT, "\txorpd %%xmm1, %%xmm1\n");
                fprintf(OUT, "\tsubsd %%xmm0, %%xmm1\n");   /* 0 - y */
                fprintf(OUT, "\tmovsd %%xmm1, %%xmm0\n");
                store_float(q->x);
            } else {
                load_int(q->y, "%rax");
                fprintf(OUT, "\tnegq %%rax\n");
                store_int(q->x);
            }
            break;
        }
        case Q_NOT: {                         /* !e：(e==0)，结果 int */
            load_int(q->y, "%rax");
            fprintf(OUT, "\ttestq %%rax, %%rax\n");
            fprintf(OUT, "\tsete %%al\n");
            fprintf(OUT, "\tmovzbq %%al, %%rax\n");
            store_int(q->x);
            break;
        }
        case Q_I2F: {                         /* (float)y：cvtsi2sd */
            load_int(q->y, "%rax");
            fprintf(OUT, "\tcvtsi2sdq %%rax, %%xmm0\n");
            store_float(q->x);
            break;
        }
        case Q_LDX: {                         /* x = y[z]：leaq 基址进 r10、
                                                 下标进 rax，一条变址访存取元素
                                                 ——x86 寻址模式的主场 */
            char b[160];
            addr_str(q->y, b, sizeof b);
            fprintf(OUT, "\tleaq %s, %%r10\n", b);
            load_int(q->z, "%rax");
            if (type_of(q->y) == TY_FLOAT) {  /* 基名的类型 = 元素类型 */
                fprintf(OUT, "\tmovsd (%%r10,%%rax,8), %%xmm0\n");
                store_float(q->x);
            } else {
                fprintf(OUT, "\tmovq (%%r10,%%rax,8), %%rax\n");
                store_int(q->x);
            }
            break;
        }
        case Q_STX: {
            char b[160];
            addr_str(q->y, b, sizeof b);
            fprintf(OUT, "\tleaq %s, %%r10\n", b);
            load_int(q->z, "%rax");
            if (type_of(q->x) == TY_FLOAT) {
                load_float(q->x, "%xmm0");
                fprintf(OUT, "\tmovsd %%xmm0, (%%r10,%%rax,8)\n");
            } else {
                load_int(q->x, "%rcx");
                fprintf(OUT, "\tmovq %%rcx, (%%r10,%%rax,8)\n");
            }
            break;
        }

        case Q_PARAM: {                       /* 实参压暂存栈：嵌套调用的
                                                 实参求值会让外层的先入栈、
                                                 内层压其上，故按深度游标 */
            int stg = byte_off(nslot_total + stg_depth);
            if (type_of(q->y) == TY_FLOAT) {
                load_float(q->y, "%xmm0");
                fprintf(OUT, "\tmovsd %%xmm0, %d(%%rbp)\n", stg);
            } else {
                load_int(q->y, "%rax");
                fprintf(OUT, "\tmovq %%rax, %d(%%rbp)\n", stg);
            }
            stg_depth++;
            break;
        }
        case Q_CALL: {
            int cf = find_fn(q->y);
            if (cf < 0) {
                fprintf(stderr, "internal error: 找不到函数 %s\n", q->y);
                exit(2);
            }
            const IrFunc *callee = &P->funcs[cf];
            /* 本组实参占暂存栈顶部 argc 格；实参类型 = 形参类型
             * （parser 已在实参上插入 i2f，两边静态一致） */
            int base = stg_depth - q->argc;
            for (int k = 0; k < q->argc && k < 4; k++) {
                int stg = byte_off(nslot_total + base + k);
                if (type_of(callee->params[k]) == TY_FLOAT)
                    fprintf(OUT, "\tmovsd %d(%%rbp), %%xmm%d\n", stg, k);
                else
                    fprintf(OUT, "\tmovq %d(%%rbp), %s\n", stg, ARGREG[k]);
            }
            for (int k = 4; k < q->argc; k++) {
                int stg = byte_off(nslot_total + base + k);
                int outa = 32 + 8 * (k - 4);
                if (type_of(callee->params[k]) == TY_FLOAT)
                    fprintf(OUT, "\tmovsd %d(%%rbp), %%xmm5\n"
                                 "\tmovsd %%xmm5, %d(%%rsp)\n", stg, outa);
                else
                    fprintf(OUT, "\tmovq %d(%%rbp), %%rax\n"
                                 "\tmovq %%rax, %d(%%rsp)\n", stg, outa);
            }
            char m[128];
            mangle(m, sizeof m, q->y);
            fprintf(OUT, "\tcall fn_%s\n", m);
            stg_depth -= q->argc;             /* 这一组实参已消费 */
            if (q->x) {                       /* 返回值落结果临时 */
                if (type_of(q->x) == TY_FLOAT) store_float(q->x);
                else                           store_int(q->x);
            }
            break;
        }

        case Q_RETURN: {
            if (q->y) {
                if (type_of(q->y) == TY_FLOAT) load_float(q->y, "%xmm0");
                else                           load_int(q->y, "%rax");
            } else if (f->rettype == TY_FLOAT) {
                fprintf(OUT, "\txorpd %%xmm0, %%xmm0\n");
            } else {
                fprintf(OUT, "\txorl %%eax, %%eax\n");
            }
            fprintf(OUT, "\tmovq %%rbp, %%rsp\n");
            fprintf(OUT, "\tpopq %%rbp\n");
            fprintf(OUT, "\tret\n");
            break;
        }
        case Q_PRINT: {
            if (type_of(q->y) == TY_FLOAT) {
                load_float(q->y, "%xmm0");
                fprintf(OUT, "\tcall minic_print_float\n");
            } else {
                load_int(q->y, "%rcx");
                fprintf(OUT, "\tcall minic_print_int\n");
            }
            break;
        }
        default:
            break;
        }
    }

    /* gen 保证段尾必有 return；防御性再放一份（不可达） */
    fprintf(OUT, "\txorl %%eax, %%eax\n");
    fprintf(OUT, "\tmovq %%rbp, %%rsp\n\tpopq %%rbp\n\tret\n");
}

/* ---------------- 文件级输出 ---------------- */

int codegen_emit(const IrProgram *p, FILE *out)
{
    P = p;
    OUT = out;
    seqno = 0;

    /* 全局名字集：主段的全部赋值目标 + is_global 数组（字节大小留给
     * .comm；数组是 len*8） */
    const IrFunc *m = &p->funcs[0];
    for (int i = 0; i < m->n; i++) {
        const char *d = dest_of(&m->q[i]);
        if (d && !imm_int(d)) add_glob(d, 8);
    }
    for (int i = 0; i < p->narrays; i++)
        if (p->arrays[i].is_global)
            add_glob(p->arrays[i].name, p->arrays[i].len * 8);

    fprintf(out, "# minicc8 生成的 x86-64 汇编（Windows ABI，gcc 可直接汇编链接）\n");
    fprintf(out, "# 全局变量经 .comm 零初始化；浮点字面量在文件尾的 .rdata 池\n");
    fprintf(out, "\t.text\n");

    for (int fi = 0; fi < p->nfuncs; fi++) {
        if (fi) fprintf(out, "\n");
        emit_function(fi);
    }

    if (nglob) {
        for (int i = 0; i < nglob; i++) {
            char gm[128];
            mangle(gm, sizeof gm, GLOB[i].name);
            fprintf(out, "\t.comm\tG_%s, %d, 8\n", gm, GLOB[i].bytes);
        }
    }

    if (nlit) {
        fprintf(out, "\t.section .rdata,\"dr\"\n");
        fprintf(out, "\t.align 8\n");
        for (int i = 0; i < nlit; i++)
            fprintf(out, ".LIT%d:\t.double %.17g\n", i, LIT[i].val);
    }
    return 0;
}
