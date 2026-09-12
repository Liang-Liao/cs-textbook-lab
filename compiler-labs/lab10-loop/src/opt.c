/*
 * opt.c —— 四元式 IR 优化器（lab9 主角；对照龙书 8.4~8.5、9.1~9.2 节）
 *
 * 位置在 gen 与后端之间：AST → 四元式 →【本文件】→ x86-64/VM/解释器。
 * 一条铁律管全部四个 pass：**优化可以删代码、改写代码，但绝不能改变
 * 程序的可观察行为**——print 的顺序、除零崩溃的时机、全局变量的副作用。
 * run_tests.sh 让七套程序带 -O 走完四种后端并与旧期望逐字节对拍，
 * 就是这条铁律的验收方式。
 *
 * 四个 pass（外层迭代至不动点——前一轮删代码会暴露新一轮的机会）：
 *
 *   A. 基本块划分（8.4 节 leader 规则）——后面一切分析的舞台；
 *   B. 局部优化（逐块单遍）：常量折叠、常量传播、局部公共子表达式消除；
 *   C. 死代码删除：CFG 上的**反向活跃变量分析**——第 9 章数据流思想的
 *      最小完整实现（use/def 集合 + in/out 方程 + 不动点迭代），
 *      删除"定义点不活跃"的系统临时；
 *   D. 窥孔：删 goto-到-紧邻标号、删无引用标号（lab6 练习 2/3 的兑现）。
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
static long n_fold, n_prop, n_cse, n_dce, n_peep, n_licm, n_unroll;

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

/* ---------------- 计数循环展开 ×2（直线体、偶数常量界、偶数趟数） ---------------- */

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

    /* 体区 = [bs+1, e-3]：必须直线码（无控制流/调用/数组写），
     * 且不得重定义归纳变量 ivar。展开后原步进对落回此区间，
     * 这条检查同时挡住对同一循环的二次展开。 */
    for (int i = bs + 1; i <= e - 3; i++) {
        Quad *t = &q[i];
        if (t->op == Q_LABEL || t->op == Q_GOTO || t->op == Q_IF_GOTO ||
            t->op == Q_IFF_GOTO || t->op == Q_CALL || t->op == Q_STX ||
            t->op == Q_RETURN)
            return 0;
        const char *d = dest_of(t);
        if (d && strcmp(d, ivar) == 0) return 0;
    }

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

/* ---------------- pass_loops：外提与展开的稳定化驱动 ---------------- */

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
             * 展开体都会让旧的下标失效。 */
            int ch = 0;
            int doneh[32], ndone = 0;
            for (int li = 0; li < nl && !ch; li++) {
                int dup = 0;
                for (int d = 0; d < ndone; d++)
                    if (doneh[d] == ls[li].head) { dup = 1; break; }
                if (dup) continue;
                if (ndone < 32) doneh[ndone++] = ls[li].head;

                if (licm_loop(p, f, &c, &ls[li])) ch = 1;
                else if (unroll_loop(p, f, &c, &ls[li])) ch = 1;
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
    collect_names(p);

    int any = 0;
    for (int round = 1; round <= 8; round++) {
        int ch = pass_local(p);
        if (pass_loops(p))    ch = 1;   /* lab10：先做循环级优化…… */
        if (pass_dce(p))      ch = 1;   /* ……再让 DCE 清理外提/展开暴露的死码 */
        if (pass_peephole(p)) ch = 1;
        if (!ch) break;
        any = 1;
    }
    fprintf(stderr, "[opt] 常量折叠 %ld、常量传播 %ld、CSE %ld、"
                    "死代码 %ld、窥孔 %ld、外提 %ld、展开 %ld\n",
            n_fold, n_prop, n_cse, n_dce, n_peep, n_licm, n_unroll);
    return any;
}
