/*
 * slr.c —— SLR(1) 分析器的完整实现（lab4 核心，对照龙书 4.6 节）
 *
 * 三层结构，恰好对应龙书的三个小节：
 *   1. closure/goto + 规范 LR(0) 项目集族   (4.6.1)
 *   2. 由 FOLLOW 剪枝的 ACTION/GOTO 表      (4.6.2)
 *   3. 移进-归约驱动循环                    (4.5/4.6)
 *
 * 【直觉】LR 分析器 = 下推自动机：
 *   - 状态栈上永远是"LR(0) 自动机"走过的路径；
 *   - 每个状态背后是一组项目（"读到这里的各种可能"）；
 *   - 句柄在栈顶出现时（圆点到达产生式末尾的项目），归约。
 * 自动机识别的是"活前缀"(viable prefix)——句柄左边的那段。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slr.h"
#include "grammar.h"
#include "lexer.h"

/* ============ LR(0) 项目 = [产生式编号, 圆点位置] ============ */
typedef struct { int prod, dot; } Item;

#define MAXITEMS 48          /* 单个状态里项目数的上限（本文法远用不到） */
#define MAXSTATES 512
#define SYMRANGE (NT_BASE + NT_COUNT)
#define STACKCAP 8192        /* 驱动器状态栈/符号栈容量 */

typedef struct {
    Item items[MAXITEMS];
    int  n;
} ItemSet;

static ItemSet states[MAXSTATES];  /* 规范项目集族：每个元素一个状态 */
static int     nstates;
static int     trans[MAXSTATES][SYMRANGE];   /* trans[s][X] = goto(s,X)，-1 无 */

/* ---- 项目集基本操作 ---- */
static int itemset_has(const ItemSet *s, Item it)
{
    for (int i = 0; i < s->n; i++)
        if (s->items[i].prod == it.prod && s->items[i].dot == it.dot)
            return 1;
    return 0;
}

static int itemset_equal(const ItemSet *a, const ItemSet *b)
{
    if (a->n != b->n) return 0;
    /* 项目插入时已去重，n 相同则逐个对应即可（顺序按插入序，需再核对） */
    for (int i = 0; i < a->n; i++)
        if (!itemset_has(b, a->items[i])) return 0;
    return 1;
}

/* 圆点右边的符号（没有则返回 -2=已完成的项） */
static int after_dot(Item it)
{
    Prod *pr = &PRODS[it.prod];
    if (it.dot >= pr->n) return -2;
    return pr->rhs[it.dot];
}

/* closure(I)（龙书 4.6.1）：
 * 若 [A→α.Bβ] 在 I 中，把所有 [B→.γ] 加进来，直到不动点。 */
static void closure(ItemSet *s)
{
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < s->n; i++) {
            int X = after_dot(s->items[i]);
            if (X < NT_BASE) continue;      /* 终结符或完成项不用闭包 */
            for (int p = 0; p < NPRODS; p++)
                if (PRODS[p].lhs == X && !itemset_has(s, (Item){p, 0})) {
                    if (s->n >= MAXITEMS) {
                        fprintf(stderr, "internal error: 单状态项目数超上限 %d"
                                        "（文法过大）\n", MAXITEMS);
                        exit(1);
                    }
                    s->items[s->n++] = (Item){p, 0};
                    changed = 1;
                }
        }
    }
}

/* goto(I, X)：I 中圆点恰在 X 前的项目，圆点右移一格后取闭包 */
static ItemSet go(const ItemSet *I, int X)
{
    ItemSet J = {0};
    for (int i = 0; i < I->n; i++)
        if (after_dot(I->items[i]) == X)
            J.items[J.n++] = (Item){I->items[i].prod, I->items[i].dot + 1};
    closure(&J);
    return J;
}

/* ============ 1. 规范 LR(0) 项目集族（BFS，龙书 4.6.1 的 GOTO 图构造） ============ */
static void build_canonical(void)
{
    nstates = 0;
    memset(trans, -1, sizeof trans);

    ItemSet start = {0};
    start.items[start.n++] = (Item){0, 0};   /* S' → . stmt_list */
    closure(&start);
    states[nstates++] = start;

    /* 工作表：扫描每个状态，对每个可能的符号求 goto */
    for (int s = 0; s < nstates; s++) {
        for (int X = 0; X < SYMRANGE; X++) {
            ItemSet J = go(&states[s], X);
            if (J.n == 0) continue;
            int found = -1;
            for (int k = 0; k < nstates; k++)
                if (itemset_equal(&states[k], &J)) { found = k; break; }
            if (found < 0) {
                if (nstates >= MAXSTATES) {
                    fprintf(stderr, "internal error: 状态数超上限\n");
                    exit(1);
                }
                states[nstates++] = J;
                found = nstates - 1;
            }
            trans[s][X] = found;
        }
    }
}

/* ============ 2. SLR(1) ACTION/GOTO 表（龙书 4.6.2 的构造规则） ============
 * 对每个状态 I：
 *   [A→α.aβ] 且 goto(I,a)=J   => ACTION[I,a] = shift J
 *   [A→α.] 且 a ∈ FOLLOW(A)   => ACTION[I,a] = reduce A→α
 *   [S'→S.]                   => ACTION[I,EOF] = accept
 *   GOTO[I,B] = goto(I,B)     （B 非终结符）
 * 同一格出现 shift 和 reduce => shift/reduce 冲突（SLR 剪不掉）；
 * 两个 reduce => reduce/reduce 冲突（文法有真问题）。 */
typedef enum { AC_ERR = 0, AC_SHIFT, AC_REDUCE, AC_ACCEPT } ActKind;

typedef struct { ActKind kind; int arg; } Action;

static Action ACTION[MAXSTATES][T_ERROR + 1];
static int    n_sr_conflicts, n_rr_conflicts;

static void set_action(int st, int term, ActKind kind, int arg, const char *why)
{
    Action old = ACTION[st][term];
    if (old.kind == AC_ERR) {           /* 空白：直接填 */
        ACTION[st][term] = (Action){kind, arg};
        return;
    }
    if (old.kind == kind && old.arg == arg) return;  /* 相同项，忽略 */

    if (old.kind == AC_SHIFT && kind == AC_REDUCE) {
        /* 悬空 else 一类的冲突：yacc/bison 的默认是优先移进。
         * 对 if-else 文法，"移进"恰好就是 C 语义（else 绑最近的 if）。 */
        n_sr_conflicts++;
        fprintf(stderr,
            "[SLR 冲突 %d] state %d 在终结符 %s 上: shift %d vs reduce P%d"
            " -> 按惯例选择移进(%s)\n",
            n_sr_conflicts, st, token_name(term), old.arg, arg, why);
        return;   /* 保持 shift */
    }
    if (old.kind == AC_REDUCE && kind == AC_SHIFT) {
        /* shift 后来者：按"移进优先"覆盖 reduce */
        n_sr_conflicts++;
        fprintf(stderr,
            "[SLR 冲突 %d] state %d 在终结符 %s 上: reduce P%d vs shift %d"
            " -> 按惯例选择移进(%s)\n",
            n_sr_conflicts, st, token_name(term), old.arg, arg, why);
        ACTION[st][term] = (Action){kind, arg};
        return;
    }
    /* reduce/reduce：文法真有歧义，教学实现直接拒绝 */
    n_rr_conflicts++;
    fprintf(stderr,
        "[SLR 冲突] state %d 在终结符 %s 上: reduce P%d 与 reduce P%d 冲突"
        "（reduce/reduce 冲突，文法有真问题）\n",
        st, token_name(term), old.arg, arg);
}

static void build_table(void)
{
    n_sr_conflicts = n_rr_conflicts = 0;
    memset(ACTION, 0, sizeof ACTION);

    for (int s = 0; s < nstates; s++) {
        for (int i = 0; i < states[s].n; i++) {
            Item it = states[s].items[i];
            int X = after_dot(it);
            Prod *pr = &PRODS[it.prod];

            if (X >= 0 && X < NT_BASE) {
                /* 圆点在终结符前：移进 */
                set_action(s, X, AC_SHIFT, trans[s][X], "移进优先");
            } else if (X == -2) {
                /* 完成项 [A→α.]：在 FOLLOW(A) 上归约（SLR 的剪枝） */
                if (it.prod == 0) {          /* S'→stmt_list. ：接受 */
                    /* 防御：accept 格若已有动作同样算冲突——本文法下
                     * 该格恒空（手工核实），但绕过 set_action 的直写
                     * 不该是静默的。 */
                    if (ACTION[s][T_EOF].kind != AC_ERR) {
                        n_rr_conflicts++;
                        fprintf(stderr, "[SLR 冲突] state %d 在终结符 EOF 上:"
                                        " accept 与既有动作冲突\n", s);
                    }
                    ACTION[s][T_EOF] = (Action){AC_ACCEPT, 0};
                } else {
                    int A = pr->lhs - NT_BASE;
                    for (int t = 0; t <= T_ERROR; t++)
                        if (in_follow(A, t))
                            set_action(s, t, AC_REDUCE, it.prod,
                                       "SLR FOLLOW 剪枝");
                }
            }
            /* 圆点在非终结符前：GOTO 表已经就是 trans，不用另建 */
        }
    }
}

void slr_build(void)
{
    compute_follow();
    build_canonical();
    build_table();
}

int slr_num_states(void) { return nstates; }
int slr_num_conflicts(void) { return n_sr_conflicts + n_rr_conflicts; }
int slr_num_sr_conflicts(void) { return n_sr_conflicts; }
int slr_num_rr_conflicts(void) { return n_rr_conflicts; }

/* ============ 3. 表驱动移进-归约循环（龙书 4.5 节图 4.x） ============ */
int slr_parse(const char *src, int trace)
{
    Lexer lx;
    lexer_init(&lx, src);
    Token cur = lexer_next_clean(&lx);

    int  sstack[STACKCAP], stop = 0;      /* 状态栈 */
    char syms[STACKCAP][16];              /* 符号栈（打印用，存名字） */
    int  ssym = 0;
    sstack[stop++] = 0;

    int steps = 0, nerr = 0;
    strcpy(syms[ssym++], "$");

    for (;;) {
        steps++;
        int st = sstack[stop - 1];
        Action a = ACTION[st][cur.type];

        if (trace) {
            printf("%-3d 状态栈[", steps);
            for (int i = 0; i < stop; i++)
                printf("%s%d", i ? " " : "", sstack[i]);
            printf("]  符号栈[");
            for (int i = 0; i < ssym; i++) printf("%s", syms[i]);
            printf("]  <-- %s | ", cur.lexeme);
        }

        if (a.kind == AC_SHIFT) {
            if (trace) printf("shift %d (%s)\n", a.arg, cur.lexeme);
            if (stop >= STACKCAP || ssym >= STACKCAP) {
                fprintf(stderr, "error: 分析栈溢出（嵌套过深，上限 %d）\n",
                        STACKCAP);
                return 1;
            }
            sstack[stop++] = a.arg;
            snprintf(syms[ssym++], 16, "%s", cur.lexeme);
            cur = lexer_next_clean(&lx);
        } else if (a.kind == AC_REDUCE) {
            Prod *pr = &PRODS[a.arg];
            if (trace) {
                printf("reduce P%d: %s ->", a.arg, sym_name(pr->lhs));
                if (pr->n == 1 && pr->rhs[0] == EPS) printf(" ε");
                else for (int i = 0; i < pr->n; i++)
                    printf(" %s", sym_name(pr->rhs[i]));
                printf("\n");
            }
            /* 弹 |rhs| 个状态（ε 产生式弹 0 个），压入 GOTO[栈顶][lhs] */
            int popn = (pr->n == 1 && pr->rhs[0] == EPS) ? 0 : pr->n;
            stop  -= popn;
            ssym  -= popn;
            snprintf(syms[ssym++], 16, "%s", sym_name(pr->lhs));
            int next = trans[sstack[stop - 1]][pr->lhs];
            sstack[stop++] = next;
        } else if (a.kind == AC_ACCEPT) {
            if (trace) printf("accept\n");
            if (cur.type != T_EOF) {
                /* 表说接受但输入没完（不可能发生，防御性检查） */
                fprintf(stderr, "line %d: error: 多余的输入\n", cur.line);
                return 1;
            }
            return nerr ? 1 : 0;
        } else {
            /* ACTION 为空：语法错误。给出"栈顶期待什么"的提示 */
            fprintf(stderr,
                "line %d, col %d: error: 语法错误: 意外的 %s\n",
                cur.line, cur.col, cur.lexeme);
            return 1;   /* 教学实现：报错即停（错误恢复练习见 README） */
        }
    }
}

/* ============ 打印 ============ */

void slr_print_states(void)
{
    printf("规范 LR(0) 项目集族（共 %d 个状态）:\n", nstates);
    for (int s = 0; s < nstates; s++) {
        printf("I%d:\n", s);
        for (int i = 0; i < states[s].n; i++) {
            Item it = states[s].items[i];
            Prod *pr = &PRODS[it.prod];
            printf("    %s ->", sym_name(pr->lhs));
            if (pr->n == 1 && pr->rhs[0] == EPS) {
                /* ε 项目 [A→·ε]：圆点语义上恒在最后，按标准记法打印
                 * "A -> ε ."（旧实现打成了 "A -> . ε"） */
                printf(" ε .");
                printf("\n");
                continue;
            }
            for (int d = 0; d < pr->n; d++) {
                if (d == it.dot) printf(" .");
                printf(" %s", sym_name(pr->rhs[d]));
            }
            if (it.dot >= pr->n) printf(" .");
            printf("\n");
        }
        int any = 0;
        for (int X = 0; X < SYMRANGE; X++)
            if (trans[s][X] >= 0) any = 1;
        if (any) {
            printf("    转移: ");
            for (int X = 0; X < SYMRANGE; X++)
                if (trans[s][X] >= 0)
                    printf("--%s--> I%d  ", sym_name(X), trans[s][X]);
            printf("\n");
        }
    }
}

void slr_print_table(void)
{
    printf("SLR(1) 分析表（s# = 移进, r# = 归约, acc = 接受, . = 出错）\n");
    /* 列：用到的终结符 */
    int used[64] = {0};
    for (int s = 0; s < nstates; s++)
        for (int t = 0; t <= T_ERROR; t++)
            if (ACTION[s][t].kind != AC_ERR) used[t] = 1;

    printf("%-5s", "状态");
    for (int t = 0; t <= T_ERROR; t++)
        if (used[t]) printf("%8s", token_name(t) + 2);
    for (int nt = 1; nt < NT_COUNT; nt++)   /* GOTO 列（S' 不会出现） */
        printf("%10s", sym_name(NT_BASE + nt));
    printf("\n");

    for (int s = 0; s < nstates; s++) {
        printf("%-5d", s);
        for (int t = 0; t <= T_ERROR; t++) {
            if (!used[t]) continue;
            Action a = ACTION[s][t];
            if      (a.kind == AC_SHIFT)  printf("%7ds", a.arg);
            else if (a.kind == AC_REDUCE) printf("%7dr", a.arg);
            else if (a.kind == AC_ACCEPT) printf("%8s", "acc");
            else                          printf("%8s", ".");
        }
        for (int nt = 1; nt < NT_COUNT; nt++) {
            int g = trans[s][NT_BASE + nt];
            if (g >= 0) printf("%10d", g);
            else        printf("%10s", ".");
        }
        printf("\n");
    }
}
