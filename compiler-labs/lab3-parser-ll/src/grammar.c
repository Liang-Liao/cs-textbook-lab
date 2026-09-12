/*
 * grammar.c —— 文法数据 + FIRST/FOLLOW 不动点计算 + LL(1) 表构造
 *
 * MiniC v2 文法（注意：为了 LL(1) 可分析，全部消除了左递归——
 * 对比 lab4 会看到 LR 文法完全不需要这一步！）：
 *
 *   stmt_list → stmt stmt_list | ε
 *   stmt      → print expr ; | expr ;
 *   expr      → equality
 *   equality  → rel equality'
 *   equality' → == rel equality' | != rel equality' | ε
 *   rel       → add rel'
 *   rel'      → < add rel' | <= add rel' | > add rel' | >= add rel' | ε
 *   add       → term add'
 *   add'      → + term add' | - term add' | ε
 *   term      → factor term'
 *   term'     → * factor term' | / factor term' | % factor term' | ε
 *   factor    → ( expr ) | - factor | INT | FLOAT
 */
#include <stdio.h>
#include <string.h>
#include "grammar.h"

#define S(t)  (t)                    /* 终结符就是 TokenType 值 */
#define N(nt) (NT_BASE + (nt))       /* 非终结符编码 */

Prod PRODS[] = {
    /* 产生式右部按符号序列写；ε 用 {EPS} 占位 */
    { N(NT_STMT_LIST),   { N(NT_STMT), N(NT_STMT_LIST) }, 2, 0 },
    { N(NT_STMT_LIST),   { EPS }, 1, 1 },

    { N(NT_STMT),        { S(T_KW_PRINT), N(NT_EXPR), S(T_SEMI) }, 3, 2 },
    { N(NT_STMT),        { N(NT_EXPR), S(T_SEMI) }, 2, 3 },

    { N(NT_EXPR),        { N(NT_EQUALITY) }, 1, 4 },

    { N(NT_EQUALITY),    { N(NT_REL), N(NT_EQUALITY_P) }, 2, 5 },
    { N(NT_EQUALITY_P),  { S(T_EQ),  N(NT_REL), N(NT_EQUALITY_P) }, 3, 6 },
    { N(NT_EQUALITY_P),  { S(T_NEQ), N(NT_REL), N(NT_EQUALITY_P) }, 3, 7 },
    { N(NT_EQUALITY_P),  { EPS }, 1, 8 },

    { N(NT_REL),         { N(NT_ADD), N(NT_REL_P) }, 2, 9 },
    { N(NT_REL_P),       { S(T_LT), N(NT_ADD), N(NT_REL_P) }, 3, 10 },
    { N(NT_REL_P),       { S(T_LE), N(NT_ADD), N(NT_REL_P) }, 3, 11 },
    { N(NT_REL_P),       { S(T_GT), N(NT_ADD), N(NT_REL_P) }, 3, 12 },
    { N(NT_REL_P),       { S(T_GE), N(NT_ADD), N(NT_REL_P) }, 3, 13 },
    { N(NT_REL_P),       { EPS }, 1, 14 },

    { N(NT_ADD),         { N(NT_TERM), N(NT_ADD_P) }, 2, 15 },
    { N(NT_ADD_P),       { S(T_PLUS),  N(NT_TERM), N(NT_ADD_P) }, 3, 16 },
    { N(NT_ADD_P),       { S(T_MINUS), N(NT_TERM), N(NT_ADD_P) }, 3, 17 },
    { N(NT_ADD_P),       { EPS }, 1, 18 },

    { N(NT_TERM),        { N(NT_FACTOR), N(NT_TERM_P) }, 2, 19 },
    { N(NT_TERM_P),      { S(T_STAR),    N(NT_FACTOR), N(NT_TERM_P) }, 3, 20 },
    { N(NT_TERM_P),      { S(T_SLASH),   N(NT_FACTOR), N(NT_TERM_P) }, 3, 21 },
    { N(NT_TERM_P),      { S(T_PERCENT), N(NT_FACTOR), N(NT_TERM_P) }, 3, 22 },
    { N(NT_TERM_P),      { EPS }, 1, 23 },

    { N(NT_FACTOR),      { S(T_LPAREN), N(NT_EXPR), S(T_RPAREN) }, 3, 24 },
    { N(NT_FACTOR),      { S(T_MINUS), N(NT_FACTOR) }, 2, 25 },
    { N(NT_FACTOR),      { S(T_INT_LIT) }, 1, 26 },
    { N(NT_FACTOR),      { S(T_FLOAT_LIT) }, 1, 27 },
};
int NPRODS = (int)(sizeof PRODS / sizeof PRODS[0]);

TSet FIRST_SET[NT_COUNT];
TSet FOLLOW_SET[NT_COUNT];
int  NULLABLE[NT_COUNT];
int  LL_TABLE[NT_COUNT][T_ERROR + 1];

/* ---------------- 位集小工具 ---------------- */
static void tset_add(TSet *s, int t) { *s |= 1ull << t; }
static int  tset_has(TSet s, int t)  { return (int)(s >> t & 1); }

static int is_nt(int sym) { return sym >= NT_BASE; }
static int nt_index(int sym) { return sym - NT_BASE; }

/* 单个符号的 FIRST（不含 ε；可空性查 NULLABLE） */
static TSet sym_first(int sym, int *nullable)
{
    TSet s = 0;
    *nullable = 0;
    if (sym == EPS) { *nullable = 1; return s; }
    if (!is_nt(sym)) { tset_add(&s, sym); return s; }
    s = FIRST_SET[nt_index(sym)];
    *nullable = NULLABLE[nt_index(sym)];
    return s;
}

/* 符号序列的 FIRST：从左往右累计，遇到不可空符号就停 */
TSet seq_first(const int *syms, int n, int *nullable_out)
{
    TSet s = 0;
    int all_nullable = 1;
    for (int i = 0; i < n; i++) {
        int nb;
        TSet f = sym_first(syms[i], &nb);
        s |= f;
        if (!nb) { all_nullable = 0; break; }
    }
    *nullable_out = all_nullable;
    return s;
}

/* ---------------- FIRST/FOLLOW 不动点计算（龙书 4.4.2） ----------------
 * 反复扫描所有产生式，直到一轮下来什么都没变——教科书式的不动点迭代。 */
void compute_first_follow(void)
{
    memset(FIRST_SET, 0, sizeof FIRST_SET);
    memset(NULLABLE, 0, sizeof NULLABLE);
    memset(FOLLOW_SET, 0, sizeof FOLLOW_SET);

    /* --- FIRST 与可空性 --- */
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < NPRODS; p++) {
            Prod *pr = &PRODS[p];
            int A = nt_index(pr->lhs);
            int all_nullable;
            TSet f = seq_first(pr->rhs, pr->n, &all_nullable);

            /* FIRST(A) |= FIRST(rhs) */
            TSet old = FIRST_SET[A];
            FIRST_SET[A] |= f;
            if (all_nullable && !NULLABLE[A]) {
                NULLABLE[A] = 1;
                changed = 1;
            }
            if (FIRST_SET[A] != old) changed = 1;
        }
    }

    /* --- FOLLOW（龙书 4.4.2 的三条规则） ---
     * 1. 开始符号的 FOLLOW 含 EOF
     * 2. A→αBβ: FIRST(β)\{ε} 并入 FOLLOW(B)
     * 3. A→αB 且 β 可空(或不存在): FOLLOW(A) 并入 FOLLOW(B)   */
    tset_add(&FOLLOW_SET[NT_STMT_LIST], T_EOF);
    changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < NPRODS; p++) {
            Prod *pr = &PRODS[p];
            for (int i = 0; i < pr->n; i++) {
                int B = pr->rhs[i];
                if (!is_nt(B)) continue;
                int b = nt_index(B);

                /* β = rhs[i+1..] */
                int tail_nullable;
                TSet beta_first = seq_first(pr->rhs + i + 1,
                                            pr->n - i - 1, &tail_nullable);

                TSet old = FOLLOW_SET[b];
                FOLLOW_SET[b] |= beta_first;               /* 规则 2 */
                if (tail_nullable)
                    FOLLOW_SET[b] |= FOLLOW_SET[nt_index(pr->lhs)]; /* 规则 3 */
                if (FOLLOW_SET[b] != old) changed = 1;
            }
        }
    }
}

/* ---------------- LL(1) 分析表构造（龙书 4.4.2） ---------------- */
static int n_conflicts = 0;

void build_ll_table(void)
{
    n_conflicts = 0;
    memset(LL_TABLE, -1, sizeof LL_TABLE);

    for (int p = 0; p < NPRODS; p++) {
        Prod *pr = &PRODS[p];
        int A = nt_index(pr->lhs);

        int nullable;
        TSet f = seq_first(pr->rhs, pr->n, &nullable);

        /* 对 FIRST(α) 的每个终结符 a: M[A,a] = p */
        for (int t = 0; t <= T_ERROR; t++) {
            if (!tset_has(f, t)) continue;
            if (LL_TABLE[A][t] == -1) LL_TABLE[A][t] = p;
            else {
                fprintf(stderr,
                    "LL(1) 冲突: M[%s][%s] 已有 P%d 又要填 P%d\n",
                    sym_name(pr->lhs), token_name(t),
                    LL_TABLE[A][t], p);
                n_conflicts++;
            }
        }
        /* α 可空: 对 FOLLOW(A) 的每个终结符 b: M[A,b] = p */
        if (nullable) {
            for (int t = 0; t <= T_ERROR; t++) {
                if (!tset_has(FOLLOW_SET[A], t)) continue;
                if (LL_TABLE[A][t] == -1) LL_TABLE[A][t] = p;
                else if (LL_TABLE[A][t] != p) {
                    fprintf(stderr,
                        "LL(1) 冲突(ε): M[%s][%s] 已有 P%d 又要填 P%d\n",
                        sym_name(pr->lhs), token_name(t),
                        LL_TABLE[A][t], p);
                    n_conflicts++;
                }
            }
        }
    }
}

int ll_conflicts(void) { return n_conflicts; }

/* ---------------- 打印 ---------------- */

const char *sym_name(int sym)
{
    if (sym == EPS) return "ε";
    if (!is_nt(sym)) {
        /* 去掉 token 名的 "T_" 前缀，文法打印更清爽 */
        const char *tn = token_name(sym);
        return tn + 2;
    }
    switch (nt_index(sym)) {
    case NT_STMT_LIST:   return "stmt_list";
    case NT_STMT:        return "stmt";
    case NT_EXPR:        return "expr";
    case NT_EQUALITY:    return "equality";
    case NT_EQUALITY_P:  return "equality'";
    case NT_REL:         return "rel";
    case NT_REL_P:       return "rel'";
    case NT_ADD:         return "add";
    case NT_ADD_P:       return "add'";
    case NT_TERM:        return "term";
    case NT_TERM_P:      return "term'";
    case NT_FACTOR:      return "factor";
    }
    return "?";
}

static void print_tset(TSet s)
{
    int first = 1;
    putchar('{');
    for (int t = 0; t <= T_ERROR; t++)
        if (tset_has(s, t)) {
            printf("%s%s", first ? "" : ", ", token_name(t));
            first = 0;
        }
    putchar('}');
}

void print_first_follow(void)
{
    printf("%-12s %-6s %-44s %s\n", "非终结符", "可空", "FIRST", "FOLLOW");
    for (int i = 0; i < NT_COUNT; i++) {
        printf("%-12s %-6s ", sym_name(NT_BASE + i),
               NULLABLE[i] ? "yes" : "no");
        print_tset(FIRST_SET[i]);
        printf("  ");
        print_tset(FOLLOW_SET[i]);
        printf("\n");
    }
}

void print_ll_table(void)
{
    /* 只打印有用的行列：行 = 有表项的非终结符；列 = 表中出现过的终结符 */
    printf("LL(1) 分析表 M[A][a]（格内是产生式编号 P#）:\n");
    int used[64] = {0};
    for (int a = 0; a < NT_COUNT; a++)
        for (int t = 0; t <= T_ERROR; t++)
            if (LL_TABLE[a][t] != -1) used[t] = 1;

    printf("%-12s", "");
    for (int t = 0; t <= T_ERROR; t++)
        if (used[t]) printf("%8s", token_name(t) + 2);
    printf("\n");
    for (int a = 0; a < NT_COUNT; a++) {
        int any = 0;
        for (int t = 0; t <= T_ERROR; t++)
            if (LL_TABLE[a][t] != -1) any = 1;
        if (!any) continue;
        printf("%-12s", sym_name(NT_BASE + a));
        for (int t = 0; t <= T_ERROR; t++) {
            if (!used[t]) continue;
            if (LL_TABLE[a][t] == -1) printf("%8s", ".");
            else printf("%7s%d", "P", LL_TABLE[a][t]);
        }
        printf("\n");
    }

    printf("\n产生式表:\n");
    for (int p = 0; p < NPRODS; p++) {
        printf("  P%-2d %s ->", p, sym_name(PRODS[p].lhs));
        for (int i = 0; i < PRODS[p].n; i++)
            printf(" %s", sym_name(PRODS[p].rhs[i]));
        printf("\n");
    }
}
