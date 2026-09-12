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
static long n_fold, n_prop, n_cse, n_dce, n_peep;

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

/* ---------------- 入口：外层迭代至不动点 ---------------- */

int opt_run(IrProgram *p)
{
    P = p;
    n_fold = n_prop = n_cse = n_dce = n_peep = 0;

    int any = 0;
    for (int round = 1; round <= 8; round++) {
        int ch = pass_local(p);
        if (pass_dce(p))       ch = 1;
        if (pass_peephole(p))  ch = 1;
        if (!ch) break;
        any = 1;
    }
    fprintf(stderr, "[opt] 常量折叠 %ld、常量传播 %ld、CSE %ld、"
                    "死代码 %ld、窥孔 %ld\n",
            n_fold, n_prop, n_cse, n_dce, n_peep);
    return any;
}
