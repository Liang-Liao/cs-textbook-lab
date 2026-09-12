/*
 * opt.c —— 四元式 IR 优化器（lab9 主角；对照龙书 8.4~8.5、9.1~9.2 节）
 *
 * 位置在 gen 与后端之间：AST → 四元式 →【本文件】→ x86-64/VM/解释器。
 * 一条铁律管全部 pass：**优化可以删代码、改写代码，但绝不能改变
 * 程序的可观察行为**——print 的顺序、除零崩溃的时机、全局变量的副作用。
 * run_tests.sh 让七套程序带 -O 走完四种后端并与旧期望逐字节对拍，
 * 就是这条铁律的验收方式。
 *
 * 各 pass（外层迭代至不动点——前一轮删代码会暴露新一轮的机会）：
 *
 *   A. 基本块划分（8.4 节 leader 规则）——后面一切分析的舞台；
 *   B. 局部优化（逐块单遍）：常量折叠、常量传播、局部公共子表达式消除；
 *   C. 死代码删除：CFG 上的**反向活跃变量分析**——第 9 章数据流思想的
 *      最小完整实现（use/def 集合 + in/out 方程 + 不动点迭代），
 *      删除"定义点不活跃"的系统临时；
 *   D. 窥孔：删 goto-到-紧邻标号、删无引用标号（lab6 练习 2/3 的兑现）；
 *   E. 循环级（lab10，第 10 章）：支配树 → 自然循环 → 不变量外提 /
 *      计数循环展开 ×2；
 *   F. 依赖分析与合法性驱动的变换（lab11，第 11 章）：仿射下标识别 →
 *      距离向量 → 展开加"携带依赖门禁"、完美嵌套循环交换；
 *      观测点 -deps（依赖向量清单）与 -space（迭代空间可视化）
 *      也实现在本文件末尾。
 *
 * 【为什么只在基本块内做折叠/传播/CSE】块内是顺序执行，"上一次算过
 * a+b"才有确定答案；控制流的汇合会让缓存失效。跨块分析本 lab 只在
 * 死代码删除这一处用数据流方程实现（Pass C）。
 *
 * 【保守性清单】每一条都是语义决定：
 *   - CALL 清空全部缓存：被调函数可能读写任何全局变量；
 *   - STX 清掉同基名数组的全部 LDX 缓存：两个不同下标表达式可能运行期
 *     同值（a[i] 与 a[j] 当 i==j），精确追踪不值得；
 *   - 整数除/模的**零除数不折叠**：`print 1 / 0;` 必须保留运行时带行
 *     号报错的语义——优化器无权把错误"优化"没；
 *   - 浮点折叠用同一套 C double 运算：位级结果与不折叠时完全一致，
 *     %g 打印不变；
 *   - 死代码只删 temps 名册里的名字（单一定义、无别名、用户不可见）；
 *     用户变量的赋值即使没人读也不删——它可能藏着一次除零崩溃。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opt.h"

#define MAX_LABELS_OPT 4096         /* 同 gen.c 的标号上限 */

/* 删除标记：占住位置，块处理完后统一压实 */
#define Q_DEAD ((QuadOp)127)

static const IrProgram *P;

/* 统计（stderr 摘要用） */
static long n_fold, n_prop, n_cse, n_dce, n_peep, n_licm, n_unroll, n_swap;

/* ---------------- 小工具 ---------------- */

static char *str_dup(const char *s)
{
    char *p = malloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

/* 立即数 = 可选负号 + 数字开头（负号来自优化器折叠的常量文本） */
static int imm_num(const char *s)
{
    if (s[0] == '-') s++;
    return s[0] >= '0' && s[0] <= '9';
}
static int imm_int(const char *s)  { return imm_num(s); }
static int imm_float(const char *s)
{
    return imm_int(s) && strpbrk(s, ".eE") != NULL;
}

/* 是否系统临时（temps 名册里的名字；名册由 gen 的 newtemp 维护） */
static int is_temp(const char *s)
{
    for (int i = 0; i < P->ntemps; i++)
        if (strcmp(P->temps[i], s) == 0) return 1;
    return 0;
}

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

/* 四元式读到的临时操作数（活跃分析与死代码只追踪系统临时） */
static void uses_of(const Quad *q, const char **u, int *nu)
{
    *nu = 0;
    switch (q->op) {
    case Q_BINOP: case Q_IF_GOTO:
        u[(*nu)++] = q->y; u[(*nu)++] = q->z; break;
    case Q_ASSIGN: case Q_NEG: case Q_NOT: case Q_I2F:
    case Q_PARAM: case Q_PRINT: case Q_IFF_GOTO:
        u[(*nu)++] = q->y; break;
    case Q_LDX:
        u[(*nu)++] = q->z; break;      /* y 是数组基名，永不是临时 */
    case Q_STX:
        u[(*nu)++] = q->z; u[(*nu)++] = q->x; break;
    case Q_RETURN:
        if (q->y) { u[(*nu)++] = q->y; }
        break;
    default:
        break;                          /* GOTO/LABEL/CALL 无临时操作数 */
    }
}

/* 折叠结果的文本格式：整数 %ld；浮点 %.17g 并保证带 '.'/'e'
 * （与 gen 的 float_text 同规则——执行器靠记法分辨类型）。
 * 必须 17 位有效数字：%g 只有 6 位，会把 0.1+0.2 的真值
 * 0.30000000000000004 折叠成文本 "0.3"，再折叠 == 比较时结果
 * 翻转——优化改变了可观察行为（铁律）。17 位保证 double 往返精确。 */
static void fmt_long(char *buf, size_t cap, long v)
{
    snprintf(buf, cap, "%ld", v);
}
static void fmt_float(char *buf, size_t cap, double d)
{
    int n = snprintf(buf, cap, "%.17g", d);
    if (n >= 0 && (size_t)n < cap && !strpbrk(buf, ".e"))
        snprintf(buf + n, cap - (size_t)n, ".0");
}

/* ---------------- 基本块划分（Pass A，龙书 8.4 的 leader 规则） ----------------
 * leader[i] != 0 表示指令 i 是某块的入口：
 *   ① 段的第一条；② 跳转目标的 LABEL；③ 跳转指令的下一条。 */

static int leaders_of(const IrFunc *f, char *leader)
{
    memset(leader, 0, (size_t)f->n);
    if (f->n == 0) return 0;
    leader[0] = 1;
    for (int i = 0; i < f->n; i++) {
        const Quad *q = &f->q[i];
        if (q->op == Q_LABEL) leader[i] = 1;
        if ((q->op == Q_GOTO || q->op == Q_IF_GOTO || q->op == Q_IFF_GOTO)
            && i + 1 < f->n)
            leader[i + 1] = 1;
    }
    int nb = 0;
    for (int i = 0; i < f->n; i++) nb += leader[i];
    return nb;
}

/* 块 b 的最后一条指令下标（bstart 为各块入口下标的数组） */
static int bend_of(const IrFunc *f, const int *bstart, int nb, int b)
{
    return (b + 1 < nb) ? bstart[b + 1] - 1 : f->n - 1;
}

/* ---------------- Pass B：局部优化（折叠 / 传播 / CSE，逐块单遍） ---------------- */

#define TBL_CAP 128

/* 常量表：名字 → 已知立即数文本（值与记法一体，直接替换操作数） */
static struct { const char *name; const char *text; } KNOWN[TBL_CAP];
static int nknown;

/* 可用表达式表：(op,bop,y,z) → 已有结果名。键里的 y/z 是替换后的规范
 * 操作数，让相同子表达式对上同一个键。 */
typedef struct { QuadOp op; TokenType bop; const char *y, *z, *res; } CEnt;
static CEnt AVAIL[TBL_CAP];
static int navail;

static void kill_known(const char *name)
{
    for (int i = 0; i < nknown; i++)
        if (strcmp(KNOWN[i].name, name) == 0) {
            KNOWN[i] = KNOWN[--nknown];
            return;
        }
}

static void add_known(const char *name, const char *text)
{
    kill_known(name);
    if (nknown >= TBL_CAP) return;
    KNOWN[nknown].name = name;
    KNOWN[nknown].text = text;
    nknown++;
}

/* 名字被重定义：它的常量性作废；以它为操作数的可用表达式也作废。
 * （NEG/NOT/I2F 条目的 z 为 NULL，比较时按空串处理。） */
static void kill_name(const char *name)
{
    for (int i = 0; i < navail; ) {
        const char *az = AVAIL[i].z ? AVAIL[i].z : "";
        if (strcmp(AVAIL[i].y, name) == 0 || strcmp(az, name) == 0 ||
            strcmp(AVAIL[i].res, name) == 0)
            AVAIL[i] = AVAIL[--navail];
        else i++;
    }
    kill_known(name);
}

static void kill_all(void) { nknown = navail = 0; }

/* 操作数替换：已知常量就换成立即数文本 */
static const char *subst(const char *s)
{
    for (int i = 0; i < nknown; i++)
        if (strcmp(KNOWN[i].name, s) == 0) {
            n_prop++;
            return KNOWN[i].text;
        }
    return s;
}

/* 尝试折叠一条纯运算四元式。成功则改写成 ASSIGN 并返回 1。
 * 除零守卫：整除/取模的常量零除数一律不折——错误也是语义。 */
static int try_fold(Quad *q)
{
    char b[64];
    const char *y = q->y, *z = q->z;

    switch (q->op) {
    case Q_I2F:
        if (!imm_int(y)) return 0;
        fmt_float(b, sizeof b, strtod(y, NULL));
        break;
    case Q_NEG:
        if (!imm_int(y)) return 0;
        if (imm_float(y)) fmt_float(b, sizeof b, -strtod(y, NULL));
        else              fmt_long(b, sizeof b, -strtol(y, NULL, 10));
        break;
    case Q_NOT:
        if (!imm_int(y)) return 0;
        fmt_long(b, sizeof b, imm_float(y) ? strtod(y, NULL) == 0.0
                                           : strtol(y, NULL, 10) == 0);
        break;
    case Q_BINOP: {
        if (!imm_int(y) || !imm_int(z)) return 0;
        int flt = imm_float(y) || imm_float(z);
        double dy = strtod(y, NULL), dz = strtod(z, NULL);
        if (q->bop == T_LT || q->bop == T_LE || q->bop == T_GT ||
            q->bop == T_GE || q->bop == T_EQ || q->bop == T_NEQ) {
            /* 比较：结果一律 int 0/1。整型必须按 long 精确比——经 double
             * 中转在 |v|>2^53 时丢精度，会与 vm.c compare（整型整比）、
             * 原生 cmpq 以及不带 -O 的行为分岔，破坏"带 -O 输出仍与期望
             * 逐字节一致"的对拍铁律（此前只修了 vm.c，折叠器漏同步） */
            int r;
            if (flt) {
                switch (q->bop) {
                case T_LT:  r = dy <  dz; break;
                case T_LE:  r = dy <= dz; break;
                case T_GT:  r = dy >  dz; break;
                case T_GE:  r = dy >= dz; break;
                case T_EQ:  r = dy == dz; break;
                default:    r = dy != dz; break;
                }
            } else {
                long vy = strtol(y, NULL, 10), vz = strtol(z, NULL, 10);
                switch (q->bop) {
                case T_LT:  r = vy <  vz; break;
                case T_LE:  r = vy <= vz; break;
                case T_GT:  r = vy >  vz; break;
                case T_GE:  r = vy >= vz; break;
                case T_EQ:  r = vy == vz; break;
                default:    r = vy != vz; break;
                }
            }
            fmt_long(b, sizeof b, r);
            break;
        }
        if (flt) {
            double r = 0;
            switch (q->bop) {
            case T_PLUS:  r = dy + dz; break;
            case T_MINUS: r = dy - dz; break;
            case T_STAR:  r = dy * dz; break;
            case T_SLASH: r = dy / dz; break;
            default: return 0;              /* 浮点取模已被前端拒绝 */
            }
            fmt_float(b, sizeof b, r);
        } else {
            long vy = strtol(y, NULL, 10), vz = strtol(z, NULL, 10), r;
            if ((q->bop == T_SLASH || q->bop == T_PERCENT) && vz == 0)
                return 0;                   /* ★ 零除数不折叠 */
            switch (q->bop) {
            case T_PLUS:  r = vy + vz; break;
            case T_MINUS: r = vy - vz; break;
            case T_STAR:  r = vy * vz; break;
            case T_SLASH: r = vy / vz; break;
            case T_PERCENT: r = vy % vz; break;
            default: return 0;
            }
            fmt_long(b, sizeof b, r);
        }
        break;
    }
    default:
        return 0;
    }

    q->op = Q_ASSIGN;
    q->y = str_dup(b);
    q->z = NULL;
    n_fold++;
    return 1;
}

static int pass_local(IrProgram *p)
{
    int changed = 0;

    for (int fi = 0; fi < p->nfuncs; fi++) {
        IrFunc *f = &p->funcs[fi];
        if (f->n == 0) continue;
        char *leader = calloc((size_t)f->n + 1, 1);
        leaders_of(f, leader);

        for (int start = 0; start < f->n; start++) {
            if (!leader[start]) continue;
            int end = start + 1;
            while (end < f->n && !leader[end]) end++;
            end--;                              /* 本块 = [start, end] */

            kill_all();                         /* 进新块：状态清零 */
            for (int i = start; i <= end; i++) {
                Quad *q = &f->q[i];

                switch (q->op) {
                case Q_CALL:                    /* 可能改全局：全清最稳 */
                    kill_all();
                    continue;
                case Q_STX:                     /* a[i]=e：同基名的元素
                                                 * 缓存全部失效（下标可能
                                                 * 运行期同值，保守处理） */
                    q->x = subst(q->x);
                    q->z = subst(q->z);
                    for (int k = 0; k < navail; ) {
                        if (AVAIL[k].op == Q_LDX &&
                            strcmp(AVAIL[k].y, q->y) == 0)
                            AVAIL[k] = AVAIL[--navail];
                        else k++;
                    }
                    continue;
                case Q_PARAM:
                    q->y = subst(q->y);
                    continue;
                case Q_PRINT:
                    q->y = subst(q->y);
                    continue;
                case Q_RETURN:
                    if (q->y) q->y = subst(q->y);
                    continue;
                case Q_ASSIGN: {                /* x = y：源若是常量文本
                                                 * 则 x 成为已知常量 */
                    q->y = subst(q->y);
                    kill_name(q->x);            /* 旧值相关缓存全部失效 */
                    if (imm_int(q->y)) add_known(q->x, q->y);
                    continue;
                }
                case Q_BINOP: case Q_NEG: case Q_NOT: case Q_I2F:
                case Q_LDX:
                    break;                      /* 走下面的统一流程 */
                default:
                    continue;                   /* LABEL/GOTO/IF* 不碰 */
                }

                /* ---- 纯运算与数组读：传播 → 折叠 → CSE ----
                 * 注意顺序：先作废"以 x 为操作数/结果"的全部旧缓存
                 * （x 马上要被重定义），再折叠、再查表登记。 */
                q->y = subst(q->y);
                if (q->op == Q_BINOP || q->op == Q_LDX) q->z = subst(q->z);

                if (!try_fold(q)) {
                    int hit = -1;
                    kill_name(q->x);
                    for (int k = 0; k < navail; k++) {
                        if (AVAIL[k].op != q->op || AVAIL[k].bop != q->bop)
                            continue;
                        if (strcmp(AVAIL[k].y, q->y) == 0 &&
                            strcmp(AVAIL[k].z ? AVAIL[k].z : "",
                                   q->z ? q->z : "") == 0) {
                            hit = k; break;
                        }
                    }
                    if (hit >= 0) {             /* 复用现成结果 */
                        q->op = Q_ASSIGN;
                        q->y = subst(AVAIL[hit].res);
                        q->z = NULL;
                        n_cse++;
                        changed = 1;
                        if (imm_int(q->y)) add_known(q->x, q->y);
                        continue;
                    }
                    if (navail < TBL_CAP) {     /* 登记新的可用表达式 */
                        AVAIL[navail].op  = q->op;
                        AVAIL[navail].bop = q->bop;
                        AVAIL[navail].y   = q->y;
                        AVAIL[navail].z   = q->z;
                        AVAIL[navail].res = q->x;
                        navail++;
                    }
                    continue;
                }

                changed = 1;                    /* 折叠成功：结果成常量 */
                kill_name(q->x);
                add_known(q->x, q->y);
            }

            start = end;                        /* 循环末尾 ++ 跳到下一块 */
        }
        free(leader);
    }
    return changed;
}

/* ---------------- Pass C：死代码删除（反向活跃变量分析入门） ---------------- */

#define TSET_MAX 512
typedef struct { const char *v[TSET_MAX]; int n; } TSet;

static void tset_add(TSet *s, const char *name)
{
    if (!name || !is_temp(name)) return;    /* 只追踪系统临时 */
    for (int i = 0; i < s->n; i++)
        if (strcmp(s->v[i], name) == 0) return;
    /* 满了不能静默丢名：丢掉的若是活跃临时，DCE 会误删其定义点
     * （错译而非崩溃）——fail-fast。 */
    if (s->n >= TSET_MAX) {
        fprintf(stderr, "internal error: 活跃集超过 %d 个临时（单函数同时"
                        "活跃的临时过多），拒绝优化\n", TSET_MAX);
        exit(2);
    }
    s->v[s->n++] = name;
}
static void tset_del(TSet *s, const char *name)
{
    for (int i = 0; i < s->n; i++)
        if (strcmp(s->v[i], name) == 0) {
            s->v[i] = s->v[--s->n];
            return;
        }
}
static int tset_in(const TSet *s, const char *name)
{
    for (int i = 0; i < s->n; i++)
        if (strcmp(s->v[i], name) == 0) return 1;
    return 0;
}
static void tset_union(TSet *dst, const TSet *src)
{
    for (int i = 0; i < src->n; i++) tset_add(dst, src->v[i]);
}
static int tset_eq(const TSet *a, const TSet *b)
{
    if (a->n != b->n) return 0;
    for (int i = 0; i < a->n; i++)
        if (!tset_in(b, a->v[i])) return 0;
    return 1;
}

/* 数据流方程（对临时名的活跃变量分析；backward may-analysis）：
 *   use[b] = 在 b 中先于任何定值就被读的临时
 *   def[b] = 在 b 中被定值的临时
 *   out[b] = ∪ in[后继]
 *   in[b]  = use[b] ∪ (out[b] − def[b])
 * 反复迭代直到两轮之间集合不再变化——这就是"不动点"。临时的活跃 =
 * 今后还会被读到；定义点不活跃 ⇒ 这条定义是死代码。 */
static int pass_dce(IrProgram *p)
{
    int changed = 0;

    for (int fi = 0; fi < p->nfuncs; fi++) {
        IrFunc *f = &p->funcs[fi];
        if (f->n == 0) continue;
        char *leader = calloc((size_t)f->n + 1, 1);
        int nb = leaders_of(f, leader);

        int *bstart = calloc((size_t)nb + 1, sizeof(int));
        for (int i = 0, b = 0; i < f->n; i++)
            if (leader[i]) bstart[b++] = i;

        /* 标号 → 所在块号（求跳转后继用） */
        static int lblblk[MAX_LABELS_OPT];
        for (int L = 0; L < MAX_LABELS_OPT; L++) lblblk[L] = -1;
        for (int b = 0; b < nb; b++) {
            int e = bend_of(f, bstart, nb, b);
            for (int i = bstart[b]; i <= e; i++)
                if (f->q[i].op == Q_LABEL && f->q[i].label < MAX_LABELS_OPT)
                    lblblk[f->q[i].label] = b;
        }

        /* 每块的 use/def */
        TSet *use = calloc((size_t)nb, sizeof(TSet));
        TSet *def = calloc((size_t)nb, sizeof(TSet));
        TSet *in  = calloc((size_t)nb, sizeof(TSet));
        TSet *out = calloc((size_t)nb, sizeof(TSet));
        for (int b = 0; b < nb; b++) {
            int e = bend_of(f, bstart, nb, b);
            for (int i = bstart[b]; i <= e; i++) {
                const Quad *q = &f->q[i];
                const char *u[3]; int nu;
                uses_of(q, u, &nu);
                for (int k = 0; k < nu; k++)
                    if (!tset_in(&def[b], u[k])) tset_add(&use[b], u[k]);
                const char *d = dest_of(q);
                if (d && is_temp(d)) tset_add(&def[b], d);
            }
        }

        /* 不动点迭代（逆序处理让回边快速收敛） */
        for (int iter = 0; iter < 10000; iter++) {
            int stable = 1;
            for (int b = nb - 1; b >= 0; b--) {
                int e = bend_of(f, bstart, nb, b);
                const Quad *last = &f->q[e];

                TSet newout = { .n = 0 };
                if (last->op == Q_GOTO) {
                    if (last->label >= 0 && last->label < MAX_LABELS_OPT &&
                        lblblk[last->label] >= 0)
                        tset_union(&newout, &in[lblblk[last->label]]);
                } else if (last->op == Q_IF_GOTO ||
                           last->op == Q_IFF_GOTO) {
                    if (b + 1 < nb) tset_union(&newout, &in[b + 1]);
                    if (last->label >= 0 && last->label < MAX_LABELS_OPT &&
                        lblblk[last->label] >= 0)
                        tset_union(&newout, &in[lblblk[last->label]]);
                } else if (last->op != Q_RETURN && b + 1 < nb) {
                    tset_union(&newout, &in[b + 1]);
                }

                if (!tset_eq(&newout, &out[b])) { stable = 0; out[b] = newout; }

                TSet newin = use[b];
                for (int k = 0; k < out[b].n; k++)
                    if (!tset_in(&def[b], out[b].v[k]))
                        tset_add(&newin, out[b].v[k]);
                if (!tset_eq(&newin, &in[b])) { stable = 0; in[b] = newin; }
            }
            if (stable) break;
        }

        /* 逐块向后扫：定义点不活跃的临时纯运算删除。
         * live 从 out[b] 出发倒退；删除的指令其操作数不再注入 live。 */
        for (int b = 0; b < nb; b++) {
            int e = bend_of(f, bstart, nb, b);
            TSet live = out[b];
            for (int i = e; i >= bstart[b]; i--) {
                Quad *q = &f->q[i];
                const char *d = dest_of(q);
                int pure = q->op == Q_ASSIGN || q->op == Q_BINOP ||
                           q->op == Q_NEG || q->op == Q_NOT ||
                           q->op == Q_I2F || q->op == Q_LDX;
                if (d && pure && !imm_int(d) && is_temp(d) &&
                    !tset_in(&live, d)) {
                    q->op = Q_DEAD;
                    n_dce++;
                    changed = 1;
                    continue;
                }
                if (d) tset_del(&live, d);
                const char *u[3]; int nu;
                uses_of(q, u, &nu);
                for (int k = 0; k < nu; k++) tset_add(&live, u[k]);
            }
        }

        /* 压实被标记删除的四元式 */
        int w = 0;
        for (int i = 0; i < f->n; i++)
            if (f->q[i].op != Q_DEAD) f->q[w++] = f->q[i];
        f->n = w;

        free(leader); free(bstart);
        free(use); free(def); free(in); free(out);
    }
    return changed;
}

/* ---------------- Pass D：窥孔 ---------------- */

static int pass_peephole(IrProgram *p)
{
    int changed = 0;
    for (int fi = 0; fi < p->nfuncs; fi++) {
        IrFunc *f = &p->funcs[fi];
        for (int stab = 0; stab < 100; stab++) {
            int ch = 0;
            /* goto 到紧邻标号：跳与不跳都落在同一条指令上 */
            for (int i = 0; i < f->n - 1; i++) {
                if (f->q[i].op == Q_GOTO && f->q[i + 1].op == Q_LABEL &&
                    f->q[i].label == f->q[i + 1].label) {
                    memmove(&f->q[i], &f->q[i + 1],
                            (size_t)(f->n - i - 1) * sizeof(Quad));
                    f->n--; ch++; n_peep++;
                }
            }
            /* 无引用标号：所有跳转都不再指向它 */
            for (int i = 0; i < f->n; i++) {
                if (f->q[i].op != Q_LABEL) continue;
                int refs = 0;
                for (int j = 0; j < f->n; j++) {
                    if (f->q[j].op == Q_GOTO || f->q[j].op == Q_IF_GOTO ||
                        f->q[j].op == Q_IFF_GOTO)
                        if (f->q[j].label == f->q[i].label) refs++;
                }
                if (refs == 0) {
                    memmove(&f->q[i], &f->q[i + 1],
                            (size_t)(f->n - i - 1) * sizeof(Quad));
                    f->n--; i--; ch++; n_peep++;
                }
            }
            if (!ch) break;
            changed = 1;
        }
    }
    return changed;
}

/* ================= lab10 新增：CFG / 支配树 / 自然循环 / LICM / 展开 =================
 *
 * 循环优化的三步曲，全部建立在"基本块 + 后继边"的 CFG 之上：
 *   1. 支配关系 dom[b] = 支配 b 的块集合——沿 CFG 前向迭代到不动点；
 *   2. 回边 b→h（h ∈ dom[b]）→ 自然循环 = {h} ∪ 从 b 沿前驱回溯、遇 h 止；
 *   3. 在自然循环上做两件事：
 *      - LICM 不变量外提：操作数在环内无重定义的纯运算移入 preheader；
 *      - 计数循环展开 ×2：规范形态（i<N、步进+1、直线体、偶数界）复制体。
 *
 * 【preheader 的插法】直接把 [LABEL Lpre; 外提码; GOTO Lh] 插在线性序列中
 * header 的 LABEL 之前——顺序执行天然先落入 preheader；函数里所有指向 Lh
 * 的跳转按"所在块是否属于循环"分流：环外改指 Lpre，环内闩边保持指向 Lh。
 *
 * 【保守性】循环体内含 CALL 则整环放弃外提/展开（被调方可能改任何全局，
 * 精确判定留给 lab12 的 mod/ref）；展开要求体是直线码且只含标量运算。
 */

static void ir_insert(IrFunc *f, int pos, const Quad *ins, int cnt)
{
    if (f->n + cnt > f->cap) {
        int cap = f->cap ? f->cap : 64;
        while (cap < f->n + cnt) cap *= 2;
        f->q = realloc(f->q, (size_t)cap * sizeof *f->q);
        f->cap = cap;
    }
    memmove(&f->q[pos + cnt], &f->q[pos],
            (size_t)(f->n - pos) * sizeof(Quad));
    memcpy(&f->q[pos], ins, (size_t)cnt * sizeof(Quad));
    f->n += cnt;
}

typedef struct {
    int nb;
    int *bstart;                /* 块入口下标 */
    char *leader;
    int *lblblk;                /* 标号号 → 块号（-1 = 无） */
    int (*succ)[4];             /* 后继块号（升序去重，最多 4） */
    int *nsucc;
} CFG;

static void cfg_free(CFG *c)
{
    free(c->bstart); free(c->leader); free(c->lblblk); free(c->succ); free(c->nsucc);
    memset(c, 0, sizeof *c);
}

static void cfg_add_succ(CFG *c, int b, int s)
{
    for (int i = 0; i < c->nsucc[b]; i++)
        if (c->succ[b][i] == s) return;         /* 去重 */
    if (c->nsucc[b] < 4) c->succ[b][c->nsucc[b]++] = s;
}

static void cfg_build(const IrFunc *f, CFG *c)
{
    memset(c, 0, sizeof *c);
    char *tmp = calloc((size_t)f->n + 1, 1);
    c->nb = leaders_of(f, tmp);                 /* 先拿块数 */
    c->leader = tmp;
    c->bstart = calloc((size_t)c->nb + 1, sizeof(int));
    for (int i = 0, b = 0; i < f->n; i++)
        if (c->leader[i]) c->bstart[b++] = i;

    c->lblblk = malloc(sizeof(int) * MAX_LABELS_OPT);
    for (int L = 0; L < MAX_LABELS_OPT; L++) c->lblblk[L] = -1;
    for (int b = 0; b < c->nb; b++) {
        int e = bend_of(f, c->bstart, c->nb, b);
        for (int i = c->bstart[b]; i <= e; i++)
            if (f->q[i].op == Q_LABEL && f->q[i].label < MAX_LABELS_OPT)
                c->lblblk[f->q[i].label] = b;
    }

    c->succ  = calloc((size_t)c->nb, sizeof(*c->succ));
    c->nsucc = calloc((size_t)c->nb, sizeof(int));
    for (int b = 0; b < c->nb; b++) {
        const Quad *last = &f->q[bend_of(f, c->bstart, c->nb, b)];
        switch (last->op) {
        case Q_GOTO:
            if (last->label >= 0 && last->label < MAX_LABELS_OPT &&
                c->lblblk[last->label] >= 0)
                cfg_add_succ(c, b, c->lblblk[last->label]);
            break;
        case Q_IF_GOTO: case Q_IFF_GOTO:
            if (b + 1 < c->nb) cfg_add_succ(c, b, b + 1);
            if (last->label >= 0 && last->label < MAX_LABELS_OPT &&
                c->lblblk[last->label] >= 0)
                cfg_add_succ(c, b, c->lblblk[last->label]);
            break;
        case Q_RETURN:
            break;
        default:
            if (b + 1 < c->nb) cfg_add_succ(c, b, b + 1);
            break;
        }
    }
}

static int cfg_is_pred(const CFG *c, int p, int b)
{
    for (int s = 0; s < c->nsucc[p]; s++)
        if (c->succ[p][s] == b) return 1;
    return 0;
}

/* 支配关系：dom[b*nb+k]=1 表示 k 支配 b。迭代方程
 *   dom[entry] = {entry};  dom[b] = {b} ∪ ∩_{p∈pred(b)} dom[p]
 * 初始化：entry 只支配自己；其余块初始假定为全集。 */
static void dominators(const CFG *c, unsigned char *dom)
{
    int nb = c->nb;
    /* dom 矩阵按 nb*nb 增长，细化用的暂存数组开在栈上（512 上限）。
     * 超限明确放弃循环优化并提示，而不是栈溢出。 */
    if (nb > 512) {
        fprintf(stderr, "[opt] 函数基本块数 %d 超过 512，跳过循环级优化\n", nb);
        return;
    }
    for (int b = 0; b < nb; b++)
        for (int k = 0; k < nb; k++)
            dom[b * nb + k] = (unsigned char)(b == 0 ? (k == 0) : 1);
    for (int changed = 1; changed; ) {
        changed = 0;
        for (int b = 1; b < nb; b++) {
            unsigned char tmp[512];
            int have = 0;
            for (int p = 0; p < nb; p++) {
                if (!cfg_is_pred(c, p, b)) continue;
                if (!have) { memcpy(tmp, &dom[p * nb], (size_t)nb); have = 1; }
                else
                    for (int k = 0; k < nb; k++) tmp[k] &= dom[p * nb + k];
            }
            if (!have) memcpy(tmp, &dom[0], (size_t)nb);   /* 无前驱：不变 */
            tmp[b] = 1;
            if (memcmp(tmp, &dom[b * nb], (size_t)nb) != 0) {
                memcpy(&dom[b * nb], tmp, (size_t)nb);
                changed = 1;
            }
        }
    }
}

/* 自然循环清单（每条回边一条；同头多回边会出现重复条目，调用方按头去重） */
typedef struct {
    int head, latch;
    unsigned char *inloop;      /* nb 字节：块是否属于本循环 */
} Loop;

static Loop *find_loops(const IrFunc *f, const CFG *c,
                        const unsigned char *dom, int *nout)
{
    (void)f;
    Loop *ls = calloc((size_t)c->nb + 1, sizeof(Loop));
    int n = 0;
    for (int b = 0; b < c->nb; b++)
        for (int s = 0; s < c->nsucc[b]; s++) {
            int h = c->succ[b][s];
            if (!dom[b * c->nb + h]) continue;          /* 非回边 */
            Loop *L = &ls[n++];
            L->head = h; L->latch = b;
            L->inloop = calloc((size_t)c->nb, 1);
            L->inloop[h] = 1;
            int *stk = malloc((size_t)(c->nb + 1) * sizeof(int)), top = 0;
            if (b != h && !L->inloop[b]) { L->inloop[b] = 1; stk[top++] = b; }
            while (top) {
                int x = stk[--top];
                for (int pr = 0; pr < c->nb; pr++)
                    if (cfg_is_pred(c, pr, x) && !L->inloop[pr]) {
                        L->inloop[pr] = 1;
                        stk[top++] = pr;
                    }
            }
            free(stk);
        }
    *nout = n;
    return ls;
}
static void loops_free(Loop *ls, int n)
{
    for (int i = 0; i < n; i++) free(ls[i].inloop);
    free(ls);
}

/* 全程序已知名字集（铸造新临时名时避让） */
typedef struct { const char **v; int n, cap; } Names;
static Names KNAMES;

static void names_add(Names *ks, const char *s)
{
    for (int i = 0; i < ks->n; i++)
        if (strcmp(ks->v[i], s) == 0) return;
    if (ks->n >= ks->cap) {
        ks->cap = ks->cap ? ks->cap * 2 : 256;
        ks->v = realloc(ks->v, (size_t)ks->cap * sizeof(char *));
    }
    ks->v[ks->n++] = s;
}
static int names_has(const Names *ks, const char *s)
{
    for (int i = 0; i < ks->n; i++)
        if (strcmp(ks->v[i], s) == 0) return 1;
    return 0;
}
static void collect_names(IrProgram *p)
{
    memset(&KNAMES, 0, sizeof KNAMES);
    for (int i = 0; i < p->ntypes; i++)  names_add(&KNAMES, p->types[i].name);
    for (int i = 0; i < p->narrays; i++) names_add(&KNAMES, p->arrays[i].name);
    for (int fi = 0; fi < p->nfuncs; fi++) {
        IrFunc *f = &p->funcs[fi];
        for (int j = 0; j < f->nparams; j++) names_add(&KNAMES, f->params[j]);
        for (int i = 0; i < f->n; i++) {
            const Quad *q = &f->q[i];
            const char *cand[3];
            int nc = 0;
            cand[nc++] = dest_of(q);
            if (q->op != Q_CALL) cand[nc++] = q->y;
            cand[nc++] = q->z;
            for (int k = 0; k < nc; k++)
                if (cand[k] && !imm_int(cand[k])) names_add(&KNAMES, cand[k]);
        }
    }
}
/* 铸造一个全新的临时名（避开一切已知名字），登记进 temps 名册 */
static const char *mint_temp(void)
{
    char b[16];
    int i = 0;
    do snprintf(b, sizeof b, "t%d", ++i);
    while (names_has(&KNAMES, b));
    const char *r = str_dup(b);
    names_add(&KNAMES, r);
    return r;                       /* 名册登记由调用方完成（见 unroll） */
}
static int next_label(const IrProgram *p)
{
    int mx = 0;
    for (int fi = 0; fi < p->nfuncs; fi++) {
        const IrFunc *f = &p->funcs[fi];
        for (int i = 0; i < f->n; i++)
            if (f->q[i].label > mx) mx = f->q[i].label;
    }
    return mx + 1;
}

/* ---------------- LICM 不变量外提（保守版） ---------------- */

static int licm_loop(IrProgram *p, IrFunc *f, const CFG *c, const Loop *L)
{
    /* 环内含 CALL：被调方可能改任何全局，整环放弃（lab12 的 mod/ref
     * 会把这条规则精确化）。 */
    for (int b = 0; b < c->nb; b++) {
        if (!L->inloop[b]) continue;
        int e = bend_of(f, c->bstart, c->nb, b);
        for (int i = c->bstart[b]; i <= e; i++)
            if (f->q[i].op == Q_CALL) return 0;
    }

    /* 环内重定义名字集：候选的操作数一旦出现在其中即非不变量 */
    Names defs = {0};
    for (int b = 0; b < c->nb; b++) {
        if (!L->inloop[b]) continue;
        int e = bend_of(f, c->bstart, c->nb, b);
        for (int i = c->bstart[b]; i <= e; i++) {
            const char *d = dest_of(&f->q[i]);
            if (d) names_add(&defs, d);
        }
    }

    /* 收集候选：纯运算（不含除/模——避免把除零崩溃提前到循环之前）、
     * 结果是系统临时、两个操作数都不在环内重定义集中。立即数天然不变。 */
    int idx[128], nc = 0;
    for (int b = 0; b < c->nb; b++) {
        if (!L->inloop[b]) continue;
        int e = bend_of(f, c->bstart, c->nb, b);
        for (int i = c->bstart[b]; i <= e; i++) {
            Quad *q = &f->q[i];
            int ok = q->op == Q_NEG || q->op == Q_NOT || q->op == Q_I2F ||
                     (q->op == Q_BINOP && q->bop != T_SLASH &&
                      q->bop != T_PERCENT);
            if (!ok || !is_temp(q->x)) continue;
            if ((q->y && imm_int(q->y) == 0 && names_has(&defs, q->y)) ||
                (q->z && imm_int(q->z) == 0 && names_has(&defs, q->z)))
                continue;
            if (nc < 128) idx[nc++] = i;
        }
    }
    free(defs.v);
    if (nc == 0) return 0;

    /* 外部指向循环头的跳转改写到 preheader（环内闩边保持不动） */
    int Lh = f->q[c->bstart[L->head]].label;
    int Lpre = next_label(p);
    for (int i = 0; i < f->n; i++) {
        Quad *q = &f->q[i];
        if ((q->op == Q_GOTO || q->op == Q_IF_GOTO || q->op == Q_IFF_GOTO) &&
            q->label == Lh) {
            int b = 0;
            while (b < c->nb &&
                   !(c->bstart[b] <= i &&
                     i <= bend_of(f, c->bstart, c->nb, b)))
                b++;
            if (b == c->nb || !L->inloop[b]) q->label = Lpre;
        }
    }

    /* 组装 preheader：LABEL Lpre + 外提四元式（保持相对顺序）+ GOTO Lh。
     * 关键顺序：**先倒序摘除候选、再插入**——候选全部位于插入点之后，
     * 摘除不影响插入点的下标；反过来先插入会让旧下标整体偏移，删错指令。 */
    Quad ins[130];
    memset(&ins[0], 0, sizeof ins[0]);
    ins[0].op = Q_LABEL; ins[0].label = Lpre; ins[0].line = 1;
    for (int k = 0; k < nc; k++) ins[1 + k] = f->q[idx[k]];
    memset(&ins[nc + 1], 0, sizeof ins[nc + 1]);
    ins[nc + 1].op = Q_GOTO; ins[nc + 1].label = Lh; ins[nc + 1].line = 1;
    for (int k = nc - 1; k >= 0; k--) {
        memmove(&f->q[idx[k]], &f->q[idx[k] + 1],
                (size_t)(f->n - idx[k] - 1) * sizeof(Quad));
        f->n--;
    }
    ir_insert(f, c->bstart[L->head], ins, nc + 2);
    n_licm += nc;
    return 1;
}

/* ---------------- 计数循环展开 ×2（直线体、偶数常量界） ---------------- */

static int body_dep_free(const IrFunc *f, int lo, int hi);   /* lab11 依赖门禁 */

/* 展开的安全性依赖"归纳变量在循环入口的值已知"。从循环头向前做一次
 * 有界反向扫描，找最近的 `ivar = 立即数`：
 *   - 无条件 GOTO 与 LABEL 直接跳过——for 的转发块、LICM 刚插的
 *     preheader 都是这种纯转发形态，跨过去才摸得到真正的 init；
 *   - 遇到条件跳转立即放弃——再往前找到的定值与入口值不再必然一致；
 *   - 找到的最近定值若不是常量赋值同样放弃。
 * 任何放弃都只损失这一次展开机会，不损失正确性。返回 0 = 初值未知。 */
static int loop_entry_const(const IrFunc *f, int hs, const char *ivar, long *out)
{
    const Quad *q = f->q;
    int lab[8], nlab = 0, found = -1;
    long v = 0;
    for (int k = hs - 1; k >= 0 && k >= hs - 48 && found < 0; k--) {
        const Quad *t = &q[k];
        if (t->op == Q_IF_GOTO || t->op == Q_IFF_GOTO || t->op == Q_RETURN)
            return 0;                       /* 跨过汇合点：路径相关，放弃 */
        if (t->op == Q_LABEL) {
            if (nlab >= 8) return 0;        /* 转发层数异常：保守放弃 */
            lab[nlab++] = t->label;
            continue;
        }
        const char *d = dest_of(t);
        if (d && strcmp(d, ivar) == 0) {
            if (t->op == Q_ASSIGN && imm_int(t->y)) {
                v = strtol(t->y, NULL, 10);
                found = k;
            }
            break;                          /* 最近定值非立即数：放弃 */
        }
        /* 其余四元式（含 GOTO、普通运算）照单越过 */
    }
    if (found < 0) return 0;
    /* 窗口内越过的标号若被"更早位置"的跳转引用，说明存在绕过该定值的
     * 旁路入口（如 if (p) goto Lmid; Lmid: 处 i 未被赋值），放弃 */
    for (int j = 0; j < found; j++)
        for (int m = 0; m < nlab; m++)
            if ((q[j].op == Q_GOTO || q[j].op == Q_IF_GOTO ||
                 q[j].op == Q_IFF_GOTO) && q[j].label == lab[m])
                return 0;
    *out = v;
    return 1;
}

/* 克隆体铸的新名必须继承原名的静态类型：后端（codegen 的 type_of）
 * 靠 IrProgram.types 选 int/float 指令路径，缺登记的名字一律按 int
 * 兜底——浮点体一经展开就会被 movq/imulq 按整型位型错译（且只有
 * 原生引擎错，解释器们按值的 isfloat 走，四引擎对拍当场指证）。
 * 类型表满与 tset_add 同款 fail-fast。 */
static void note_type_like(IrProgram *p, const char *old, const char *nw)
{
    for (int i = 0; i < p->ntypes; i++)
        if (strcmp(p->types[i].name, old) == 0) {
            for (int j = 0; j < p->ntypes; j++)
                if (strcmp(p->types[j].name, nw) == 0) return;
            if (p->ntypes >= 1024) {
                fprintf(stderr, "error: 类型登记表超过上限 %d\n", 1024);
                exit(2);
            }
            p->types[p->ntypes].name = nw;
            p->types[p->ntypes].ty   = p->types[i].ty;
            p->ntypes++;
            return;
        }
    /* 原名本就未登记：新名同样不登记，与后端的 int 兜底保持一致 */
}

static int unroll_loop(IrProgram *p, IrFunc *f, const CFG *c, const Loop *L)
{
    int hb = L->head;
    int hs = c->bstart[hb], he = bend_of(f, c->bstart, c->nb, hb);
    Quad *q = f->q;

    /* 头形态：LABEL Lh; if i < N goto Lbody（N 为正偶数常量） */
    if (hs + 1 > he || q[hs].op != Q_LABEL || q[hs + 1].op != Q_IF_GOTO ||
        q[hs + 1].bop != T_LT)
        return 0;
    const char *ivar = q[hs + 1].y, *ntxt = q[hs + 1].z;
    if (!imm_int(ntxt)) return 0;
    long N = strtol(ntxt, NULL, 10);
    if (N % 2 != 0 || N <= 0) return 0;
    /* 趟数 = N − 入口初值。只看上界 N 的奇偶不够：初值为奇数时趟数为
     * 奇数，展开后最后一对的第二个体会以 i==N 越界多执行一次
     * （for (i=1;i<9;…) 会把体跑出 [1,8] 范围）。初值未知同样不展开。 */
    long s;
    if (!loop_entry_const(f, hs, ivar, &s)) return 0;
    if ((N - s) % 2 != 0) return 0;
    int Lbody = q[hs + 1].label;
    int bb = (Lbody >= 0 && Lbody < MAX_LABELS_OPT) ? c->lblblk[Lbody] : -1;
    if (bb < 0 || bb != L->latch) return 0;   /* 体与闩同块 ⇒ 直线体前提 */

    int bs = c->bstart[bb], e = bend_of(f, c->bstart, c->nb, bb);
    if (e - bs < 4) return 0;
    if (q[bs].op != Q_LABEL || q[bs].label != Lbody) return 0;
    /* 步进对紧贴闩边：… t = i + 1 ; i = t ; goto Lh */
    if (q[e - 1].op != Q_ASSIGN || q[e - 2].op != Q_BINOP ||
        q[e - 2].bop != T_PLUS || strcmp(q[e - 2].y, ivar) != 0 ||
        strcmp(q[e - 2].z, "1") != 0 || q[e - 1].y != q[e - 2].x)
        return 0;

    /* 体区 = [bs+1, e-3]：必须直线码（无控制流/调用/return），
     * 且不得重定义归纳变量 ivar。展开后原步进对落回此区间，
     * 这条检查同时挡住对同一循环的二次展开。
     * lab11：数组写 Q_STX **解禁**——是否安全交给下面的依赖门禁判定
     * （lab10 时代一刀切禁止，是因为那时还没有依赖分析这把尺子）。 */
    for (int i = bs + 1; i <= e - 3; i++) {
        Quad *t = &q[i];
        if (t->op == Q_LABEL || t->op == Q_GOTO || t->op == Q_IF_GOTO ||
            t->op == Q_IFF_GOTO || t->op == Q_CALL ||
            t->op == Q_RETURN)
            return 0;
        const char *d = dest_of(t);
        if (d && strcmp(d, ivar) == 0) return 0;
    }

    /* lab11 依赖门禁（第 11 章"合法性驱动的变换"）：收集体内全部访存，
     * 只要存在**跨迭代的记忆依赖**（距离向量任一分量非零）或无法解析
     * 的访存下标，就保守放弃展开。无携带依赖时带数组写的体也能安全
     * 展开（如逐元素初始化），而 a[i]=a[i-1] 这类流依赖循环被挡在门外
     * ——两章在这里呼应。 */
    if (!body_dep_free(f, bs + 1, e - 3)) return 0;

    /* 克隆 [bs+1 .. e-1]（体 + 步进对）：**区域内定义**的临时统一换
     * 新名（映射表保证克隆内部引用一致）；用户变量与区域外定值的
     * 临时（典型：LICM 刚外提到 preheader 的不变量，体内只剩对其
     * 的使用——它们的值跨迭代不变）一律原样共享。若把后者也改名，
     * 克隆体会引用一个从未定义的新临时，直接读出垃圾值。 */
    int lo = bs + 1, hi = e - 1, cnt = hi - lo + 1;
    Quad *cl = malloc((size_t)cnt * sizeof(Quad));
    Names rdefs = {0};
    for (int k = lo; k <= hi; k++) {
        const char *d = dest_of(&q[k]);
        if (d) names_add(&rdefs, d);
    }
    struct { const char *old, *nw; } map[128];
    int nmap = 0;
    for (int k = 0; k < cnt; k++) {
        cl[k] = q[lo + k];
        const char **slot[3] = { &cl[k].x, &cl[k].y, &cl[k].z };
        for (int s = 0; s < 3; s++) {
            const char *nm = *slot[s];
            if (!nm || imm_int(nm)) continue;
            int found = -1;
            for (int m = 0; m < nmap; m++)
                if (strcmp(map[m].old, nm) == 0) { found = m; break; }
            if (found < 0 && is_temp(nm) && names_has(&rdefs, nm)) {
                const char *fresh = mint_temp();
                map[nmap].old = nm;
                map[nmap].nw  = fresh;
                nmap++;
                found = nmap - 1;
            }
            if (found >= 0) *slot[s] = map[found].nw;
        }
    }
    free(rdefs.v);
    ir_insert(f, e, cl, cnt);               /* 插在闩边 goto 之前 */
    free(cl);
    for (int m = 0; m < nmap; m++)          /* 新临时登记进名册：DCE 只删
                                             * 名册内的名字，克隆体必须入册 */
        if (p->ntemps < 2048) p->temps[p->ntemps++] = map[m].nw;
    for (int m = 0; m < nmap; m++)          /* 新临时继承静态类型：否则
                                             * 浮点克隆体被后端按 int 错译 */
        note_type_like(p, map[m].old, map[m].nw);
    n_unroll++;
    return 1;
}

/* ================= lab11 新增：仿射下标识别 / 距离向量 / 循环交换 =================
 *
 * 第 11 章"依赖分析与局部性"的教学实现，全部建立在既有 CFG/自然循环
 * 机器之上：
 *
 *   1.【仿射下标识别】把访存四元式（Q_LDX/Q_STX）的下标地址逆向解析成
 *      「iv」「iv ± 常量」或纯常量（教学版口径）。二维数组先解开 gen 的
 *      行主序降糖链（t2 = t1 + 列；t1 = 行 * 列数常量），恢复出
 *      （行下标, 列下标）再逐维处理。解不出来记 unknown——分析可以弱，
 *      结论必须稳：unknown 一律阻断后续变换。
 *
 *   2.【距离向量】同一循环内两条同数组访存 A、B（A 先于 B）：设 B 在
 *      迭代 I+δ 访问的元素与 A 在迭代 I 访问的相同，逐维解
 *      iv + cA = iv + δ + cB 得 δ = cA − cB。分类按读写方向：
 *      写→读 true、读→写 anti、写→写 output、读→读 none。
 *
 *   3.【合法性驱动的变换】展开加携带依赖门禁（见 unroll_loop）；完美
 *      二重嵌套做循环交换——while 降糖后的布局天然对称，交换只需把
 *      内外两层 IF 四元式的条件互换，标号与跳转一字不动。
 *
 *   观测点 -deps / -space 也在这节末尾：两者都作用于 gen 后、-O 前
 *   的原始 IR（稳定观测点），输出纯 ASCII 便于逐字节对拍。
 */

/* 仿射式：ok=1 时 iv=NULL 表示纯常量 c；否则表示 iv + c */
typedef struct {
    int         ok;
    const char *iv;
    long        c;
} Affine;

/* 区域内被定值的名字及次数（叶子的"计数器/不变量"资格判定用）。
 * 同名最多细查前两个定义点：规范计数器 = 一次常量 init + 一次自增。 */
typedef struct { const char *name; int cnt; int q1, q2; } RDEnt;
typedef struct { RDEnt *v; int n, cap; } RegDefs;

static void regdef_add(RegDefs *rd, const char *name, int qidx)
{
    for (int i = 0; i < rd->n; i++)
        if (strcmp(rd->v[i].name, name) == 0) {
            rd->v[i].cnt++;
            if (rd->v[i].q1 < 0) rd->v[i].q1 = qidx;
            else if (rd->v[i].q2 < 0) rd->v[i].q2 = qidx;
            return;
        }
    if (rd->n >= rd->cap) {
        rd->cap = rd->cap ? rd->cap * 2 : 32;
        rd->v = realloc(rd->v, (size_t)rd->cap * sizeof(RDEnt));
    }
    rd->v[rd->n].name = name;
    rd->v[rd->n].cnt  = 1;
    rd->v[rd->n].q1   = qidx;
    rd->v[rd->n].q2   = -1;
    rd->n++;
}

static RDEnt *regdef_find(RegDefs *rd, const char *name)
{
    for (int i = 0; i < rd->n; i++)
        if (strcmp(rd->v[i].name, name) == 0) return &rd->v[i];
    return NULL;
}

/* 区域 [lo,hi] 内被定值的名字集 */
static void regdefs_build(RegDefs *rd, const IrFunc *f, int lo, int hi)
{
    memset(rd, 0, sizeof *rd);
    for (int i = lo; i <= hi; i++) {
        const char *d = dest_of(&f->q[i]);
        if (d) regdef_add(rd, d, i);
    }
}

/* 临时名 → 定义四元式下标（gen 保证临时单定义；全函数扫一遍） */
typedef struct { const char *name; int qidx; } TDefEnt;
typedef struct { TDefEnt *v; int n, cap; } DefMap;

static void defmap_build(DefMap *dm, const IrFunc *f)
{
    memset(dm, 0, sizeof *dm);
    for (int i = 0; i < f->n; i++) {
        const char *d = dest_of(&f->q[i]);
        if (!d || !is_temp(d)) continue;
        if (dm->n >= dm->cap) {
            dm->cap = dm->cap ? dm->cap * 2 : 64;
            dm->v = realloc(dm->v, (size_t)dm->cap * sizeof(TDefEnt));
        }
        dm->v[dm->n].name = d;
        dm->v[dm->n].qidx = i;
        dm->n++;
    }
}

static int defmap_find(const DefMap *dm, const char *name)
{
    for (int i = 0; i < dm->n; i++)
        if (strcmp(dm->v[i].name, name) == 0) return dm->v[i].qidx;
    return -1;
}

/* 叶子资格（距离方程对"下标变量随迭代线性变化"的前提）：
 *   - 区域内零定义：不变符号，成对消元后与迭代无关——放行；
 *   - 区域内定义呈规范计数器形态：至多一次常量 init + 恰一次
 *     "x = x ± 常量" 自增（for 降糖的 init/step 正是这对）——放行；
 *   - 其余任何定义形态一律拒绝。 */
static int leaf_ok(const IrFunc *f, const DefMap *dm, const RegDefs *rd,
                   const char *name)
{
    if (is_temp(name)) return 0;            /* 临时走定义链，不作叶子 */
    RDEnt *e = regdef_find((RegDefs *)rd, name);
    if (!e) return 1;                       /* 环外定值：区域内不变 */
    if (e->cnt > 2) return 0;
    int nstep = 0, ninit = 0;
    for (int k = 0; k < 2 && (k == 0 || e->q2 >= 0); k++) {
        int qi = k == 0 ? e->q1 : e->q2;
        const Quad *a = &f->q[qi];
        if (a->op == Q_ASSIGN && a->y && imm_num(a->y)) {
            ninit++;                        /* 常量 init（j = 0） */
            continue;
        }
        if (a->op == Q_ASSIGN && a->y) {    /* x = t，t = x ± 常量？ */
            int ti = defmap_find(dm, a->y);
            if (ti >= 0) {
                const Quad *b = &f->q[ti];
                if (b->op == Q_BINOP &&
                    (b->bop == T_PLUS || b->bop == T_MINUS) &&
                    strcmp(b->y, name) == 0 && imm_int(b->z)) {
                    nstep++;
                    continue;
                }
            }
        }
        return 0;                           /* 非规范形态 */
    }
    return nstep == 1 && ninit + nstep == e->cnt;
}

/* 仿射解析：addr = 常量 | 名字 | 名字 ± 常量 的定义链回溯（深度限 8） */
static Affine resolve_affine(const IrFunc *f, const DefMap *dm,
                             const RegDefs *rd, const char *addr, int depth)
{
    Affine a = {0, NULL, 0};
    if (!addr || depth > 8) return a;
    if (imm_num(addr)) {
        a.ok = 1;
        a.c = strtol(addr, NULL, 10);
        return a;
    }
    int di = defmap_find(dm, addr);
    if (di < 0) {                           /* 叶子：变量本身 */
        if (!leaf_ok(f, dm, rd, addr)) return a;
        a.ok = 1;
        a.iv = addr;
        return a;
    }
    const Quad *q = &f->q[di];
    if (q->op == Q_ASSIGN)                  /* 直通赋值链（含窥孔产物） */
        return resolve_affine(f, dm, rd, q->y, depth + 1);
    if (q->op != Q_BINOP || (q->bop != T_PLUS && q->bop != T_MINUS))
        return a;                           /* 乘除等：教学版不支持 */
    if (imm_int(q->z)) {                    /* name ± 常量 */
        Affine b = resolve_affine(f, dm, rd, q->y, depth + 1);
        if (!b.ok) return a;
        b.c += (q->bop == T_PLUS ? 1 : -1) * strtol(q->z, NULL, 10);
        return b;
    }
    if (q->bop == T_PLUS && imm_int(q->y)) {/* 常量 + name */
        Affine b = resolve_affine(f, dm, rd, q->z, depth + 1);
        if (!b.ok) return a;
        b.c += strtol(q->y, NULL, 10);
        return b;
    }
    return a;
}

/* 一条访存：ndim=0 表示下标无法解析（unknown）；二维已还原出行/列 */
typedef struct {
    const char *base;
    int         is_write;
    int         ndim;
    Affine      dim[2];
} MemRef;

/* 从四元式提取访存。先试二维降糖链 z: t2 = t1 + 列, t1 = 行 * 常量；
 * 不匹配再按一维直接解析。 */
static void memref_of(const IrFunc *f, const DefMap *dm, const RegDefs *rd,
                      const Quad *q, MemRef *mr)
{
    mr->base     = q->y;
    mr->is_write = q->op == Q_STX;
    mr->ndim     = 0;
    const char *z = q->z;
    if (!imm_num(z)) {
        int di2 = defmap_find(dm, z);
        if (di2 >= 0) {
            const Quad *add = &f->q[di2];
            if (add->op == Q_BINOP && add->bop == T_PLUS && !imm_num(add->z)) {
                int dri = defmap_find(dm, add->y);
                if (dri >= 0) {
                    const Quad *mul = &f->q[dri];
                    if (mul->op == Q_BINOP && mul->bop == T_STAR &&
                        imm_int(mul->z)) {
                        Affine row = resolve_affine(f, dm, rd, mul->y, 0);
                        Affine col = resolve_affine(f, dm, rd, add->z, 0);
                        if (row.ok && col.ok) {
                            mr->ndim = 2;
                            mr->dim[0] = row;
                            mr->dim[1] = col;
                            return;
                        }
                    }
                }
            }
        }
    }
    Affine a1 = resolve_affine(f, dm, rd, z, 0);
    if (a1.ok) {
        mr->ndim = 1;
        mr->dim[0] = a1;
    }
}

typedef struct { MemRef r; int qidx; } RefEnt;

/* 收集 [lo,hi] 内的全部访存（按下标序 = 程序序，S 编号依据） */
static int collect_refs(const IrFunc *f, const DefMap *dm, const RegDefs *rd,
                        int lo, int hi, RefEnt *out, int cap)
{
    int n = 0;
    for (int i = lo; i <= hi && n < cap; i++) {
        const Quad *q = &f->q[i];
        if (q->op != Q_LDX && q->op != Q_STX) continue;
        out[n].qidx = i;
        memref_of(f, dm, rd, q, &out[n].r);
        n++;
    }
    return n;
}

enum { DEP_NONE = 0, DEP_TRUE, DEP_ANTI, DEP_OUTPUT, DEP_UNKNOWN };

typedef struct { int known; long d[2]; } Dist;

static const char *dep_name(int k)
{
    switch (k) {
    case DEP_TRUE:    return "true";
    case DEP_ANTI:    return "anti";
    case DEP_OUTPUT:  return "output";
    case DEP_UNKNOWN: return "unknown";
    default:          return "none";
    }
}

/* 有向依赖 A→B（程序序 A 先于 B）。same_stmt=1 表示自依赖对：
 * 零距离的自身访问是恒真冗余，不算依赖。 */
static int classify_pair(const MemRef *a, const MemRef *b, int same_stmt,
                         Dist *od)
{
    od->known = 0;
    od->d[0] = od->d[1] = 0;
    if (strcmp(a->base, b->base) != 0) return DEP_NONE;
    int wr = (a->is_write ? 2 : 0) | (b->is_write ? 1 : 0);
    if (wr == 0) return DEP_NONE;           /* 读读不相依 */
    if (a->ndim == 0 || b->ndim == 0 || a->ndim != b->ndim)
        return DEP_UNKNOWN;
    long d[2] = {0, 0};
    for (int k = 0; k < a->ndim; k++) {
        const Affine *x = &a->dim[k], *y = &b->dim[k];
        if (!x->iv && !y->iv) {             /* 双常量：同址才有依赖 */
            if (x->c != y->c) return DEP_NONE;
            d[k] = 0;
        } else if (x->iv && y->iv && strcmp(x->iv, y->iv) == 0) {
            d[k] = x->c - y->c;             /* δ = cA − cB */
        } else {
            return DEP_UNKNOWN;             /* 不同变量：教学版不追 */
        }
    }
    od->known = 1;
    od->d[0] = d[0];
    od->d[1] = d[1];
    if (same_stmt && d[0] == 0 && (a->ndim == 1 || d[1] == 0))
        return DEP_NONE;
    return wr == 1 ? DEP_ANTI : wr == 2 ? DEP_TRUE : DEP_OUTPUT;
}

/* 统一入口：解出 rs[i]、rs[j]（程序序 i ≤ j）之间**时间上可实现**的
 * 依赖。原始方程给出的 δ 若字典序为负，意味着真正的依赖方向反过来
 * ——后写的语句喂给了后一迭代的先读语句（典型：写喂下一迭代的读，
 * 经典真依赖）。此时按反向重解，并经 psrc/pdst 指针报告真实源/汇。
 * 零距离（δ=0）是语句序携带的同迭代依赖，天然可实现，原样返回。 */
static int dep_between(const RefEnt *rs, int i, int j, Dist *od,
                       int *psrc, int *pdst)
{
    *psrc = i;
    *pdst = j;
    Dist dd;
    int cl = classify_pair(&rs[i].r, &rs[j].r, i == j, &dd);
    if (cl == DEP_NONE || cl == DEP_UNKNOWN || !dd.known) {
        *od = dd;
        return cl;
    }
    long d1 = dd.d[0], d2 = dd.d[1];
    if (d1 > 0 || (d1 == 0 && d2 >= 0)) {   /* 已是实现方向 */
        *od = dd;
        return cl;
    }
    int cl2 = classify_pair(&rs[j].r, &rs[i].r, i == j, &dd);
    *od = dd;
    *psrc = j;
    *pdst = i;
    return cl2;
}

/* 展开门禁（unroll_loop 调用）：体区存在跨迭代记忆依赖或 unknown 即否 */
static int body_dep_free(const IrFunc *f, int lo, int hi)
{
    DefMap dm;
    defmap_build(&dm, f);
    RegDefs rd;
    regdefs_build(&rd, f, lo, hi);
    RefEnt rs[64];
    int nr = collect_refs(f, &dm, &rd, lo, hi, rs, 64);
    int ok = 1;
    for (int i = 0; i < nr && ok; i++)
        for (int j = i; j < nr && ok; j++) {
            Dist dd;
            int src, dst;
            int c = dep_between(rs, i, j, &dd, &src, &dst);
            if (c == DEP_UNKNOWN) {         /* 解不出 = 不放行 */
                ok = 0;
                break;
            }
            if (c == DEP_NONE) continue;
            if (dd.d[0] != 0 || (rs[src].r.ndim == 2 && dd.d[1] != 0)) {
                ok = 0;                     /* 携带依赖：放弃展开 */
                break;
            }
        }
    free(dm.v);
    free(rd.v);
    return ok;
}

/* ---------------- -deps：距离向量清单（stderr，ASCII 可对拍） ---------------- */

void deps_dump(const IrProgram *p)
{
    P = p;                                  /* is_temp 名册查询用 */
    for (int fi = 0; fi < p->nfuncs; fi++) {
        const IrFunc *f = &p->funcs[fi];
        fprintf(stderr, "[deps] == func %s ==\n", f->name);
        if (f->n == 0) continue;
        CFG c;
        cfg_build(f, &c);
        if (c.nb > 512) {
            fprintf(stderr, "[deps] func %s: %d blocks, skipped\n", f->name, c.nb);
            cfg_free(&c);
            continue;
        }
        unsigned char *dom = calloc((size_t)c.nb * (size_t)c.nb, 1);
        dominators(&c, dom);
        int nl = 0;
        Loop *ls = find_loops(f, &c, dom, &nl);
        int doneh[32], ndone = 0;
        for (int li = 0; li < nl; li++) {
            int dup = 0;
            for (int d = 0; d < ndone; d++)
                if (doneh[d] == ls[li].head) { dup = 1; break; }
            if (dup) continue;
            if (ndone < 32) doneh[ndone++] = ls[li].head;

            /* 环内访存按程序序收集并编 S 号。先完整收齐环内定义集再
             * 解析访存——若边走边解析，步进定义尚未入表会让计数器
             * 叶子（j = j + 1 在访存之后）被判非规范形态。 */
            DefMap dm;
            defmap_build(&dm, f);
            RegDefs rd_all;
            memset(&rd_all, 0, sizeof rd_all);
            for (int b = 0; b < c.nb; b++) {
                if (!ls[li].inloop[b]) continue;
                int e = bend_of(f, c.bstart, c.nb, b);
                for (int i = c.bstart[b]; i <= e; i++) {
                    const char *dnm = dest_of(&f->q[i]);
                    if (dnm) regdef_add(&rd_all, dnm, i);
                }
            }
            RefEnt rs[64];
            int nr = 0;
            for (int b = 0; b < c.nb; b++) {
                if (!ls[li].inloop[b]) continue;
                int e = bend_of(f, c.bstart, c.nb, b);
                for (int i = c.bstart[b]; i <= e && nr < 64; i++) {
                    const Quad *qq = &f->q[i];
                    if (qq->op == Q_LDX || qq->op == Q_STX) {
                        rs[nr].qidx = i;
                        memref_of(f, &dm, &rd_all, qq, &rs[nr].r);
                        nr++;
                    }
                }
            }
            fprintf(stderr, "[deps] loop head=B%d latch=B%d stmts=%d\n",
                   ls[li].head, ls[li].latch, nr);
            for (int i = 0; i < nr; i++)
                for (int j = i; j < nr; j++) {
                    Dist dd;
                    int src, dst;
                    int cl = dep_between(rs, i, j, &dd, &src, &dst);
                    char db[32];
                    if (cl == DEP_NONE)
                        snprintf(db, sizeof db, "-");
                    else if (!dd.known)
                        snprintf(db, sizeof db, "?");
                    else if (rs[src].r.ndim == 2)
                        snprintf(db, sizeof db, "(%ld,%ld)", dd.d[0], dd.d[1]);
                    else
                        snprintf(db, sizeof db, "%ld", dd.d[0]);
                    fprintf(stderr, "[S%d->S%d %s dist=%s %s]\n", src + 1,
                           dst + 1, rs[src].r.base, db, dep_name(cl));
                }
            free(rd_all.v);
            free(dm.v);
        }
        loops_free(ls, nl);
        free(dom);
        cfg_free(&c);
    }
}

/* ---------------- 完美二重嵌套识别（交换与 -space 共用） ----------------
 * while/for 降糖后的标准七块布局（块号相对外层头 hb；IF 与它的否定
 * GOTO 分属两块——跳转的下一条是 leader；for 的 init 降糖成"内层计数
 * 器清零"，落在外层体内、内层头之前，即 hb+2）：
 *   hb+0: LABEL Lo; if i<N goto Lbody_o             ← 外层头
 *   hb+1: goto Lendo                                 ← 外层出口跳转
 *   hb+2: LABEL Lbo; j=0                             ← 转发块（内层 init）
 *   hb+3: LABEL Li; if j<M goto Lbody_i             ← 内层头
 *   hb+4: goto Lendi
 *   hb+5: LABEL Lbj; …体…; t=j+1; j=t; goto Li      ← 体+内层步进
 *   hb+6: LABEL Lendi; s=i+1; i=s; goto Lo          ← 外层步进（闩边）
 * 各块形状逐一核对即"完美嵌套"成立：外层体内除内层循环外别无他物。
 * 另要求外层 init 是尾随在外层头之前一块里的 `i = 0`（规范形）。 */

typedef struct {
    int  valid;
    int  pos_lbl_o;             /* 外层头 LABEL 下标（交换时在其前插 ivi=0） */
    int  pos_if_o, pos_if_i;    /* 内外层 IF 四元式下标（交换时互换条件） */
    int  fwd_lo, fwd_hi;        /* 转发块里内层 init 四元式范围（LABEL 之后；
                                 * 空 = for 省略了 init 子句） */
    int  pos_step_in, pos_step_out; /* 内/外层步进对起始（BINOP / ASSIGN） */
    long no, ni;                /* 外/内层常量界 */
    long o_lo, i_lo;            /* 外/内层计数器常量起点（-space 展示用；
                                 * 交换变换额外要求两者为 0） */
    int  body_lo, body_hi;      /* 体区四元式范围（不含 LABEL/步进/goto） */
} Nest;

static void nest_find(const IrFunc *f, const CFG *c, const Loop *L, Nest *nk)
{
    memset(nk, 0, sizeof *nk);
    nk->valid = 0;
    int hb = L->head;
    if (L->latch == hb) return;
    Quad *q = f->q;

    /* 外层头：[LABEL Lo][if ivo < No goto Lbody]（IF 与 GOTO 出口分属
     * 两块——跳转指令的下一条是 leader，见 8.4 的块划分） */
    int hs = c->bstart[hb];
    if (bend_of(f, c->bstart, c->nb, hb) - hs != 1) return;
    if (q[hs].op != Q_LABEL || q[hs + 1].op != Q_IF_GOTO ||
        q[hs + 1].bop != T_LT)
        return;
    if (!imm_int(q[hs + 1].z)) return;
    long no = strtol(q[hs + 1].z, NULL, 10);
    if (no <= 0) return;
    const char *ivo = q[hs + 1].y;

    /* 外层出口 GOTO（独立单四元式块）+ 外层 init 检查 */
    if (hb + 6 >= c->nb || hb < 1) return;
    int xs = c->bstart[hb + 1];
    if (xs != bend_of(f, c->bstart, c->nb, hb + 1) || q[xs].op != Q_GOTO)
        return;
    /* 外层 init：前一块以 `ivo = 常量` 收尾（直落入外层头）。起点值
     * 记录进 o_lo——-space 展示真实区间；交换变换另行要求 0（见下）*/
    int pe = bend_of(f, c->bstart, c->nb, hb - 1);
    if (q[pe].op != Q_ASSIGN || !imm_int(q[pe].y) ||
        strcmp(q[pe].x, ivo) != 0)
        return;
    nk->o_lo = strtol(q[pe].y, NULL, 10);

    /* 转发块 = 内层 init（for 降糖落点）：只许空，或恰一条常量赋值。
     * 带副作用/非常量初值的 init 一律不认——交换会挪动它的执行时机。 */
    int fs = c->bstart[hb + 2], fe = bend_of(f, c->bstart, c->nb, hb + 2);
    if (q[fs].op != Q_LABEL) return;
    for (int i = fs + 1; i <= fe; i++) {
        QuadOp op = q[i].op;
        if (op == Q_LABEL || op == Q_GOTO || op == Q_IF_GOTO ||
            op == Q_IFF_GOTO || op == Q_CALL || op == Q_PRINT ||
            op == Q_STX || op == Q_RETURN)
            return;
    }
    if (fe > fs + 1) return;                    /* init 多于一条：非规范形 */
    if (fe == fs) return;                       /* 空 init：内层起点未知。i_lo
                                                 * 记 0 会让交换把计数器强行
                                                 * 清零重跑（如内层 j 从 2 起步
                                                 * 的循环迭代次数被改变），
                                                 * i_lo 不只是展示用，直接放弃 */

    /* 内层头：[LABEL Li][if ivi < Ni goto Lbody_i]，随后独立的出口 GOTO */
    int ihs = c->bstart[hb + 3];
    if (bend_of(f, c->bstart, c->nb, hb + 3) - ihs != 1 ||
        q[ihs].op != Q_LABEL || q[ihs + 1].op != Q_IF_GOTO ||
        q[ihs + 1].bop != T_LT)
        return;
    if (!imm_int(q[ihs + 1].z)) return;
    long ni = strtol(q[ihs + 1].z, NULL, 10);
    if (ni <= 0) return;
    const char *ivi = q[ihs + 1].y;
    if (strcmp(ivo, ivi) == 0) return;
    int xs2 = c->bstart[hb + 4];
    if (xs2 != bend_of(f, c->bstart, c->nb, hb + 4) || q[xs2].op != Q_GOTO)
        return;
    if (fe == fs + 1) {                        /* 非空 init：目标须是内层
                                                * 计数器且常量初值 */
        if (!(q[fs + 1].op == Q_ASSIGN && strcmp(q[fs + 1].x, ivi) == 0 &&
              imm_int(q[fs + 1].y)))
            return;
        nk->i_lo = strtol(q[fs + 1].y, NULL, 10);
    }

    /* 体块：LABEL 对准内层 IF 目标；尾 = 内层步进对 + 回内头的闩边 */
    int bs = c->bstart[hb + 5], be = bend_of(f, c->bstart, c->nb, hb + 5);
    if (be - bs < 4) return;                    /* 至少 LABEL+步进对+GOTO */
    if (q[bs].op != Q_LABEL || q[bs].label != q[ihs + 1].label) return;
    if (q[be].op != Q_GOTO || q[be].label != q[ihs].label) return;
    if (q[be - 1].op != Q_ASSIGN || q[be - 2].op != Q_BINOP ||
        q[be - 2].bop != T_PLUS || strcmp(q[be - 2].y, ivi) != 0 ||
        strcmp(q[be - 2].z, "1") != 0 || q[be - 1].y != q[be - 2].x)
        return;                                 /* 内层步进 j = j + 1 */

    /* 体区必须直线：控制流/调用/返回一律拒绝。CALL/PARAM 的实例顺序
     * 会随交换改变、且被调方的隐藏访存对依赖分析不可见；标号跳转则
     * 破坏"体逐实例对映"的前提（unroll 的体检查同款清单）。 */
    for (int i = bs + 1; i <= be - 3; i++)
        if (q[i].op == Q_CALL || q[i].op == Q_PARAM || q[i].op == Q_RETURN ||
            q[i].op == Q_LABEL || q[i].op == Q_GOTO ||
            q[i].op == Q_IF_GOTO || q[i].op == Q_IFF_GOTO)
            return;

    /* 外层步进块：[LABEL Lendi][s=i+1][i=s][goto Lo] */
    int os = c->bstart[hb + 6], oe = bend_of(f, c->bstart, c->nb, hb + 6);
    if (oe - os != 3) return;                   /* 恰 4 条 */
    if (q[os].op != Q_LABEL || q[os].label != q[xs2].label) return;
    if (q[os + 1].op != Q_BINOP ||
        q[os + 1].bop != T_PLUS || strcmp(q[os + 1].y, ivo) != 0 ||
        strcmp(q[os + 1].z, "1") != 0 || q[os + 2].op != Q_ASSIGN ||
        q[os + 2].y != q[os + 1].x)
        return;                                 /* 外层步进 i = i + 1 */
    if (q[os + 3].op != Q_GOTO || q[os + 3].label != q[hs].label) return;
    if (L->latch != hb + 6) return;             /* 闩边确属本循环 */

    nk->valid = 1;
    nk->pos_lbl_o = hs;
    nk->pos_if_o = hs + 1;
    nk->pos_if_i = ihs + 1;
    nk->fwd_lo = fs + 1;
    nk->fwd_hi = fe;
    nk->pos_step_in = be - 2;
    nk->pos_step_out = os + 1;
    nk->no = no;
    nk->ni = ni;
    nk->body_lo = bs + 1;
    nk->body_hi = be - 3;
}

/* 循环交换：合法性 = 全部距离向量已知且交换后不出现逆序分量。 */
static int swap_nest(IrProgram *p, IrFunc *f, const CFG *c, const Loop *L)
{
    (void)p;
    Nest nk;
    nest_find(f, c, L, &nk);
    if (!nk.valid) return 0;

    /* 交换要重写计数器维护（新内层每轮从 0 重置、新外层一次性置 0），
     * 因此只接受两维都从 0 起步的规范形——非零起点直接放弃 */
    if (nk.o_lo != 0 || nk.i_lo != 0) return 0;

    /* 体内有 print ⇒ 实例顺序改变会改变输出顺序——铁律禁止 */
    for (int i = nk.body_lo; i <= nk.body_hi; i++)
        if (f->q[i].op == Q_PRINT) return 0;

    /* 体区标量门禁：非访存四元式里的非临时名只允许两个归纳变量。
     * 数组访存（LDX/STX）交给距离向量分析，但用户标量携带的跨迭代
     * 依赖依赖分析看不见——典型是浮点归约 acc = acc + m[i][j]，
     * 交换后累加次序由行主变列主，IEEE 加法不结合，输出可观察改变。 */
    {
        const char *ivo_ = f->q[nk.pos_if_o].y, *ivi_ = f->q[nk.pos_if_i].y;
        for (int i = nk.body_lo; i <= nk.body_hi; i++) {
            const Quad *t = &f->q[i];
            if (t->op == Q_LDX || t->op == Q_STX) continue;
            const char *cand[3] = { dest_of(t), t->y, t->z };
            for (int s2 = 0; s2 < 3; s2++) {
                const char *nm = cand[s2];
                if (!nm || imm_int(nm) || is_temp(nm)) continue;
                if (strcmp(nm, ivo_) == 0 || strcmp(nm, ivi_) == 0) continue;
                return 0;                   /* 用户标量进体：交换放弃 */
            }
        }
    }

    DefMap dm;
    defmap_build(&dm, f);
    RegDefs rd;
    regdefs_build(&rd, f, nk.body_lo, nk.body_hi);
    RefEnt rs[64];
    int nr = collect_refs(f, &dm, &rd, nk.body_lo, nk.body_hi, rs, 64);
    int legal = 1;
    for (int i = 0; i < nr && legal; i++)
        for (int j = i; j < nr && legal; j++) {
            Dist dd;
            int src, dst;
            int cl = dep_between(rs, i, j, &dd, &src, &dst);
            if (cl == DEP_UNKNOWN) {        /* 解不出 = 不换 */
                legal = 0;
                break;
            }
            if (cl == DEP_NONE) continue;
            long d1 = dd.d[0];
            long d2 = rs[src].r.ndim == 2 ? dd.d[1] : 0;
            /* dep_between 保证 (d1,d2) 是原执行序下可实现的距离向量
             * （字典序非负）。交换后迭代键变 (j,i)：新序要求 d2>0，
             * 或 d2==0 且 d1>=0（零向量=语句序携带，体未动仍成立）*/
            if (d2 < 0 || (d2 == 0 && d1 < 0))
                legal = 0;
        }
    free(dm.v);
    free(rd.v);
    if (!legal) return 0;

    /* ---- 变换：互换内外层，计数器维护随之重写 ----
     * while 降糖布局高度对称，交换只需五步（全部原地改写，不挪动体）：
     *   ① 两条 IF 的（归纳变量, 界）互换——外层头改测 ivi<ni，
     *      内层头改测 ivo<no；标号与跳转目标原样复用；
     *   ② 内层步进 j=j+1 改为 i=i+1（新内层是原外层计数器）；
     *   ③ 外层步进 i=i+1 改为 j=j+1；
     *   ④ 转发块的内层 init `j=0` 换成新内层的 init `i=0`；
     *   ⑤ 外层头 LABEL 前插入一次性的 `j=0`（新外层的 init）。 */
    const char *ivo = f->q[nk.pos_if_o].y;
    const char *ivi = f->q[nk.pos_if_i].y;

    {   /* ① IF 条件互换 */
        Quad *oif = &f->q[nk.pos_if_o], *iif = &f->q[nk.pos_if_i];
        const char *ty = oif->y, *tz = oif->z;
        oif->y = iif->y;
        oif->z = iif->z;
        iif->y = ty;
        iif->z = tz;
    }
    {   /* ② 内层步进：t = j + 1 ; j = t  →  t = i + 1 ; i = t */
        Quad *bin = &f->q[nk.pos_step_in], *asg = &f->q[nk.pos_step_in + 1];
        bin->y = ivo;
        asg->x = ivo;
    }
    {   /* ③ 外层步进：s = i + 1 ; i = s  →  s = j + 1 ; j = s */
        Quad *bin = &f->q[nk.pos_step_out], *asg = &f->q[nk.pos_step_out + 1];
        bin->y = ivi;
        asg->x = ivi;
    }
    /* 零值初始化四元式（x = 0），与 gen 的标量声明同形态 */
    Quad zero_i, zero_j;
    memset(&zero_i, 0, sizeof zero_i);
    zero_i.op = Q_ASSIGN; zero_i.x = (char *)ivo; zero_i.y = "0";
    zero_i.label = -1; zero_i.line = 1;
    memset(&zero_j, 0, sizeof zero_j);
    zero_j.op = Q_ASSIGN; zero_j.x = (char *)ivi; zero_j.y = "0";
    zero_j.label = -1; zero_j.line = 1;

    /* ④ 转发块 init 替换（先删后插，位置都在外层头之后、内层头之前）*/
    if (nk.fwd_hi >= nk.fwd_lo) {
        int lo = nk.fwd_lo, hi = nk.fwd_hi;
        memmove(&f->q[lo], &f->q[hi + 1],
                (size_t)(f->n - hi - 1) * sizeof(Quad));
        f->n -= hi - lo + 1;
    }
    ir_insert(f, nk.fwd_lo, &zero_i, 1);
    /* ⑤ 新外层的一次性 init 插到外层头 LABEL 之前 */
    ir_insert(f, nk.pos_lbl_o, &zero_j, 1);
    n_swap++;
    return 1;
}

/* ---------------- -space：迭代空间 ASCII 可视化 ----------------
 *
 * 对每个完美二重嵌套打印 (i,j) 网格与依赖箭头样例。网格里 '.' 是普通
 * 迭代实例，字母是某条依赖的**源**实例样例（箭头行给出位移向量）。
 * 输出纯 ASCII 便于逐字节对拍；超过 16×16 只列依赖不画格。 */

#define SPACE_GRID_MAX 16

void space_dump(const IrProgram *p)
{
    P = p;
    for (int fi = 0; fi < p->nfuncs; fi++) {
        const IrFunc *f = &p->funcs[fi];
        if (f->n == 0) continue;
        CFG c;
        cfg_build(f, &c);
        if (c.nb > 512) { cfg_free(&c); continue; }
        unsigned char *dom = calloc((size_t)c.nb * (size_t)c.nb, 1);
        dominators(&c, dom);
        int nl = 0;
        Loop *ls = find_loops(f, &c, dom, &nl);
        int doneh[32], ndone = 0;
        for (int li = 0; li < nl; li++) {
            int dup = 0;
            for (int d = 0; d < ndone; d++)
                if (doneh[d] == ls[li].head) { dup = 1; break; }
            if (dup) continue;
            if (ndone < 32) doneh[ndone++] = ls[li].head;

            Nest nk;
            nest_find(f, &c, &ls[li], &nk);
            if (!nk.valid) continue;
            fprintf(stderr, "[space] == func %s ==\n", f->name);
            fprintf(stderr, "[space] nest i in [%ld,%ld) x j in [%ld,%ld)\n",
                   nk.o_lo, nk.no, nk.i_lo, nk.ni);

            DefMap dm;
            defmap_build(&dm, f);
            RegDefs rd;
            regdefs_build(&rd, f, nk.body_lo, nk.body_hi);
            RefEnt rs[64];
            int nr = collect_refs(f, &dm, &rd, nk.body_lo, nk.body_hi,
                                  rs, 64);

            /* 收集去重后的依赖元组（含自依赖；none/unknown 不画） */
            struct {
                int si, sj, cl;
                long d1, d2, sr, sc;
                int tagged;
                char tag;
            } tp[8];
            int nt = 0, truncated = 0;
            for (int i = 0; i < nr; i++)
                for (int j = i; j < nr; j++) {
                    Dist dd;
                    int src, dst;
                    int cl = dep_between(rs, i, j, &dd, &src, &dst);
                    if (cl == DEP_NONE || cl == DEP_UNKNOWN || !dd.known)
                        continue;
                    long d1 = dd.d[0];
                    long d2 = rs[src].r.ndim == 2 ? dd.d[1] : 0;
                    if (d1 == 0 && d2 == 0) continue;   /* 零位移不画箭头 */
                    if (nt >= 8) { truncated = 1; break; }
                    tp[nt].si = src; tp[nt].sj = dst; tp[nt].cl = cl;
                    tp[nt].d1 = d1; tp[nt].d2 = d2;
                    tp[nt].tagged = 0;
                    tp[nt].tag = (char)('A' + nt);
                    nt++;
                }

            /* 网格：给每条依赖找第一个落在格内的源实例打字母标记 */
            if (nk.no <= SPACE_GRID_MAX && nk.ni <= SPACE_GRID_MAX) {
                for (int t = 0; t < nt; t++)
                    for (long r = nk.o_lo; r < nk.no && !tp[t].tagged; r++)
                        for (long cc = nk.i_lo; cc < nk.ni && !tp[t].tagged; cc++) {
                            long r2 = r + tp[t].d1, c2 = cc + tp[t].d2;
                            if (r2 >= 0 && r2 < nk.no &&
                                c2 >= 0 && c2 < nk.ni) {
                                tp[t].tagged = 1;
                                tp[t].sr = r;
                                tp[t].sc = cc;
                            }
                        }
                fprintf(stderr, "[space]      ");
                for (long cc = 0; cc < nk.ni; cc++) fprintf(stderr, "j=%-4ld", cc);
                fprintf(stderr, "\n");
                for (long r = nk.o_lo; r < nk.no; r++) {
                    fprintf(stderr, "[space] i=%-3ld", r);
                    for (long cc = nk.i_lo; cc < nk.ni; cc++) {
                        char tag = '.';
                        for (int t = 0; t < nt; t++)
                            if (tp[t].tagged && tp[t].sr == r &&
                                tp[t].sc == cc)
                                tag = tp[t].tag;
                        fprintf(stderr, "%-5c", tag);
                    }
                    fprintf(stderr, "\n");
                }
            } else {
                fprintf(stderr, "[space] grid %ldx%ld exceeds %dx%d, omitted\n",
                       nk.no, nk.ni, SPACE_GRID_MAX, SPACE_GRID_MAX);
            }
            fprintf(stderr, "[space] arrows:\n");
            if (nt == 0)
                fprintf(stderr, "[space]   (none)\n");
            for (int t = 0; t < nt; t++) {
                char db[32];
                if (rs[tp[t].si].r.ndim == 2)
                    snprintf(db, sizeof db, "(%ld,%ld)", tp[t].d1, tp[t].d2);
                else
                    snprintf(db, sizeof db, "%ld", tp[t].d1);
                if (tp[t].tagged)
                    fprintf(stderr, "[space]   %c: S%d->S%d dist=%s %s at (i=%ld,j=%ld)\n",
                           tp[t].tag, tp[t].si + 1, tp[t].sj + 1, db,
                           dep_name(tp[t].cl), tp[t].sr, tp[t].sc);
                else
                    fprintf(stderr, "[space]   S%d->S%d dist=%s %s\n",
                           tp[t].si + 1, tp[t].sj + 1, db,
                           dep_name(tp[t].cl));
            }
            if (truncated)
                fprintf(stderr, "[space]   (more than 8 distinct deps, truncated)\n");
            free(rd.v);
            free(dm.v);
        }
        loops_free(ls, nl);
        free(dom);
        cfg_free(&c);
    }
}

/* ---------------- pass_loops：外提与展开的稳定化驱动 ---------------- */

/* 已交换过的循环头（一次 opt_run 内不重复交换）：交换合法判据对
 * 交换后的嵌套依然成立，不加这道记忆会把嵌套来回翻转不止。 */
static int swp_fi[32], swp_head[32], n_swpdone;

static int swap_already_done(int fi, int head)
{
    for (int i = 0; i < n_swpdone; i++)
        if (swp_fi[i] == fi && swp_head[i] == head) return 1;
    return 0;
}

static int pass_loops(IrProgram *p)
{
    int changed = 0;
    for (int fi = 0; fi < p->nfuncs; fi++) {
        IrFunc *f = &p->funcs[fi];
        if (f->n == 0) continue;
        for (int stab = 0; stab < 8; stab++) {
            CFG c; cfg_build(f, &c);
            unsigned char *dom = calloc((size_t)c.nb * (size_t)c.nb, 1);
            dominators(&c, dom);
            int nl = 0;
            Loop *ls = find_loops(f, &c, dom, &nl);

            /* 同一循环头只处理一次（多条回边共享头的情形去重）。
             * 每完成一次结构变换就重建 CFG 重来——插入 preheader /
             * 展开体都会让旧的下标失效。
             *
             * lab11：交换**独占一轮扫描**先行——它要求七块规范布局，
             * 而 LICM 的 preheader / 展开的复制体都会破坏布局；若与
             * 它们混在同一次访问里，内层循环的外提会抢先触发重建，
             * 外层嵌套永远轮不到交换（实测踩过的坑）。 */
            int ch = 0;
            int doneh[32], ndone = 0;
            for (int li = 0; li < nl && !ch; li++) {
                int dup = 0;
                for (int d = 0; d < ndone; d++)
                    if (doneh[d] == ls[li].head) { dup = 1; break; }
                if (dup) continue;
                if (ndone < 32) doneh[ndone++] = ls[li].head;
                if (swap_already_done(fi, ls[li].head)) continue;

                if (swap_nest(p, f, &c, &ls[li])) {
                    if (n_swpdone < 32) {       /* lab11：完美嵌套交换 */
                        swp_fi[n_swpdone] = fi;
                        swp_head[n_swpdone] = ls[li].head;
                        n_swpdone++;
                    }
                    ch = 1;
                }
            }
            if (!ch) {
                int doneh2[32], ndone2 = 0;
                for (int li = 0; li < nl && !ch; li++) {
                    int dup = 0;
                    for (int d = 0; d < ndone2; d++)
                        if (doneh2[d] == ls[li].head) { dup = 1; break; }
                    if (dup) continue;
                    if (ndone2 < 32) doneh2[ndone2++] = ls[li].head;

                    if (licm_loop(p, f, &c, &ls[li])) ch = 1;
                    else if (unroll_loop(p, f, &c, &ls[li])) ch = 1;
                }
            }
            loops_free(ls, nl);
            free(dom);
            cfg_free(&c);
            if (!ch) break;
            changed = 1;
        }
    }
    return changed;
}

/* ---------------- -cfg：CFG 与支配树/循环的文本转储 ---------------- */

void cfg_dump(const IrProgram *p)
{
    for (int fi = 0; fi < p->nfuncs; fi++) {
        const IrFunc *f = &p->funcs[fi];
        printf("== func %s ==\n", f->name);
        if (f->n == 0) { printf("  (empty)\n"); continue; }
        CFG c; cfg_build(f, &c);
        unsigned char *dom = calloc((size_t)c.nb * (size_t)c.nb, 1);
        dominators(&c, dom);
        printf("  blocks: %d\n", c.nb);
        for (int b = 0; b < c.nb; b++) {
            printf("  B%d: [%d..%d] succ={", b, c.bstart[b],
                   bend_of(f, c.bstart, c.nb, b));
            for (int s = 0; s < c.nsucc[b]; s++)
                printf("%sB%d", s ? "," : "", c.succ[b][s]);
            printf("} dom={");
            int first = 1;
            for (int k = 0; k < c.nb; k++)
                if (dom[b * c.nb + k]) {
                    printf("%sB%d", first ? "" : ",", k);
                    first = 0;
                }
            printf("}\n");
        }
        int nl = 0;
        Loop *ls = find_loops(f, &c, dom, &nl);
        for (int li = 0; li < nl; li++) {
            printf("  loop: head=B%d latch=B%d blocks={",
                   ls[li].head, ls[li].latch);
            int first = 1;
            for (int b = 0; b < c.nb; b++)
                if (ls[li].inloop[b]) {
                    printf("%sB%d", first ? "" : ",", b);
                    first = 0;
                }
            printf("}\n");
        }
        loops_free(ls, nl);
        free(dom);
        cfg_free(&c);
    }
}

/* ---------------- 入口：外层迭代至不动点 ---------------- */

int opt_run(IrProgram *p)
{
    P = p;
    n_fold = n_prop = n_cse = n_dce = n_peep = n_licm = n_unroll = 0;
    n_swap = 0;
    n_swpdone = 0;
    collect_names(p);

    int any = 0;
    for (int round = 1; round <= 8; round++) {
        int ch = pass_local(p);
        if (pass_loops(p))    ch = 1;   /* lab10/11：先做循环级优化…… */
        if (pass_dce(p))      ch = 1;   /* ……再让 DCE 清理外提/展开暴露的死码 */
        if (pass_peephole(p)) ch = 1;
        if (!ch) break;
        any = 1;
    }
    fprintf(stderr, "[opt] 常量折叠 %ld、常量传播 %ld、CSE %ld、"
                    "死代码 %ld、窥孔 %ld、外提 %ld、展开 %ld、交换 %ld\n",
            n_fold, n_prop, n_cse, n_dce, n_peep, n_licm, n_unroll, n_swap);
    return any;
}
