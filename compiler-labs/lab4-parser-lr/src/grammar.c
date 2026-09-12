#include <stdio.h>
#include <string.h>
#include "grammar.h"

#define S(t)  (t)
#define N(nt) (NT_BASE + (nt))

/* 产生式表。P0 必须是增广产生式 S'→stmt_list（驱动器靠它识别"接受"） */
Prod PRODS[] = {
    { N(NT_S_PRIME),    { N(NT_STMT_LIST) }, 1 },                          /* P0 */

    { N(NT_STMT_LIST),  { N(NT_STMT_LIST), N(NT_STMT) }, 2 },              /* P1 */
    { N(NT_STMT_LIST),  { EPS }, 1 },                                      /* P2 */

    { N(NT_STMT),       { S(T_KW_PRINT), N(NT_EXPR), S(T_SEMI) }, 3 },     /* P3 */
    { N(NT_STMT),       { N(NT_EXPR), S(T_SEMI) }, 2 },                    /* P4 */
    { N(NT_STMT),       { S(T_KW_IF), S(T_LPAREN), N(NT_EXPR), S(T_RPAREN),
                          N(NT_STMT) }, 5 },                               /* P5 */
    { N(NT_STMT),       { S(T_KW_IF), S(T_LPAREN), N(NT_EXPR), S(T_RPAREN),
                          N(NT_STMT), S(T_KW_ELSE), N(NT_STMT) }, 7 },     /* P6 */
    { N(NT_STMT),       { S(T_LBRACE), N(NT_STMT_LIST), S(T_RBRACE) }, 3 },/* P7 */
    { N(NT_STMT),       { S(T_SEMI) }, 1 },                                /* P8 */

    { N(NT_EXPR),       { N(NT_EXPR), S(T_EQ),  N(NT_REL) }, 3 },          /* P9 */
    { N(NT_EXPR),       { N(NT_EXPR), S(T_NEQ), N(NT_REL) }, 3 },          /* P10 */
    { N(NT_EXPR),       { N(NT_REL) }, 1 },                                /* P11 */

    { N(NT_REL),        { N(NT_REL), S(T_LT), N(NT_ADD) }, 3 },            /* P12 */
    { N(NT_REL),        { N(NT_REL), S(T_LE), N(NT_ADD) }, 3 },            /* P13 */
    { N(NT_REL),        { N(NT_REL), S(T_GT), N(NT_ADD) }, 3 },            /* P14 */
    { N(NT_REL),        { N(NT_REL), S(T_GE), N(NT_ADD) }, 3 },            /* P15 */
    { N(NT_REL),        { N(NT_ADD) }, 1 },                                /* P16 */

    { N(NT_ADD),        { N(NT_ADD), S(T_PLUS),  N(NT_TERM) }, 3 },        /* P17 */
    { N(NT_ADD),        { N(NT_ADD), S(T_MINUS), N(NT_TERM) }, 3 },        /* P18 */
    { N(NT_ADD),        { N(NT_TERM) }, 1 },                               /* P19 */

    { N(NT_TERM),       { N(NT_TERM), S(T_STAR),    N(NT_FACTOR) }, 3 },   /* P20 */
    { N(NT_TERM),       { N(NT_TERM), S(T_SLASH),   N(NT_FACTOR) }, 3 },   /* P21 */
    { N(NT_TERM),       { N(NT_TERM), S(T_PERCENT), N(NT_FACTOR) }, 3 },   /* P22 */
    { N(NT_TERM),       { N(NT_FACTOR) }, 1 },                             /* P23 */

    { N(NT_FACTOR),     { S(T_LPAREN), N(NT_EXPR), S(T_RPAREN) }, 3 },     /* P24 */
    { N(NT_FACTOR),     { S(T_MINUS), N(NT_FACTOR) }, 2 },                 /* P25 */
    { N(NT_FACTOR),     { S(T_INT_LIT) }, 1 },                             /* P26 */
    { N(NT_FACTOR),     { S(T_FLOAT_LIT) }, 1 },                           /* P27 */
};
int NPRODS = (int)(sizeof PRODS / sizeof PRODS[0]);

uint64_t FOLLOW_SET[NT_COUNT];

/* ---------- FIRST/FOLLOW（与 lab3 相同的不动点算法；这里只需要 FOLLOW） ---------- */

static uint64_t FIRST_SET[NT_COUNT];
static int      NULLABLE[NT_COUNT];

static int is_nt(int sym) { return sym >= NT_BASE; }
static int nt_index(int sym) { return sym - NT_BASE; }

/* 单符号 FIRST + 可空性（不含 ε 位，ε 用 NULLABLE 表示） */
static uint64_t sym_first(int sym, int *nullable)
{
    uint64_t s = 0;
    *nullable = 0;
    if (sym == EPS) { *nullable = 1; return s; }
    if (!is_nt(sym)) { s = 1ull << sym; return s; }
    s = FIRST_SET[nt_index(sym)];
    *nullable = NULLABLE[nt_index(sym)];
    return s;
}

static uint64_t seq_first(const int *syms, int n, int *nullable_out)
{
    uint64_t s = 0;
    int all = 1;
    for (int i = 0; i < n; i++) {
        int nb;
        s |= sym_first(syms[i], &nb);
        if (!nb) { all = 0; break; }
    }
    *nullable_out = all;
    return s;
}

void compute_follow(void)
{
    /* FIRST（顺带算，FOLLOW 规则 2 要用） */
    memset(FIRST_SET, 0, sizeof FIRST_SET);
    memset(NULLABLE, 0, sizeof NULLABLE);
    int changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < NPRODS; p++) {
            int A = nt_index(PRODS[p].lhs);
            int nb;
            uint64_t f = seq_first(PRODS[p].rhs, PRODS[p].n, &nb);
            if ((FIRST_SET[A] | f) != FIRST_SET[A]) { FIRST_SET[A] |= f; changed = 1; }
            if (nb && !NULLABLE[A]) { NULLABLE[A] = 1; changed = 1; }
        }
    }

    /* FOLLOW：与 lab3 相同的三条规则 */
    memset(FOLLOW_SET, 0, sizeof FOLLOW_SET);
    FOLLOW_SET[NT_S_PRIME] = 1ull << T_EOF;
    changed = 1;
    while (changed) {
        changed = 0;
        for (int p = 0; p < NPRODS; p++) {
            Prod *pr = &PRODS[p];
            for (int i = 0; i < pr->n; i++) {
                int B = pr->rhs[i];
                if (!is_nt(B)) continue;
                int b = nt_index(B);
                int tail_nb;
                uint64_t bf = seq_first(pr->rhs + i + 1, pr->n - i - 1, &tail_nb);
                uint64_t old = FOLLOW_SET[b];
                FOLLOW_SET[b] |= bf;
                if (tail_nb)
                    FOLLOW_SET[b] |= FOLLOW_SET[nt_index(pr->lhs)];
                if (FOLLOW_SET[b] != old) changed = 1;
            }
        }
    }
}

/* ---------------- 打印 ---------------- */

const char *sym_name(int sym)
{
    if (sym == EPS) return "ε";
    if (!is_nt(sym)) return token_name(sym) + 2;  /* 去掉 T_ 前缀 */
    switch (nt_index(sym)) {
    case NT_S_PRIME:   return "S'";
    case NT_STMT_LIST: return "stmt_list";
    case NT_STMT:      return "stmt";
    case NT_EXPR:      return "expr";
    case NT_REL:       return "rel";
    case NT_ADD:       return "add";
    case NT_TERM:      return "term";
    case NT_FACTOR:    return "factor";
    }
    return "?";
}

static void print_tset(uint64_t s)
{
    int first = 1;
    putchar('{');
    for (int t = 0; t <= T_ERROR; t++)
        if (s >> t & 1) {
            printf("%s%s", first ? "" : ", ", token_name(t) + 2);
            first = 0;
        }
    putchar('}');
}

void print_grammar(void)
{
    printf("MiniC v3 文法（%d 条产生式）:\n", NPRODS);
    for (int p = 0; p < NPRODS; p++) {
        printf("  P%-2d %s ->", p, sym_name(PRODS[p].lhs));
        if (PRODS[p].n == 1 && PRODS[p].rhs[0] == EPS)
            printf(" ε");
        else
            for (int i = 0; i < PRODS[p].n; i++)
                printf(" %s", sym_name(PRODS[p].rhs[i]));
        printf("\n");
    }
    printf("\nFOLLOW 集:\n");
    for (int i = 0; i < NT_COUNT; i++) {
        printf("  %-11s ", sym_name(NT_BASE + i));
        print_tset(FOLLOW_SET[i]);
        printf("\n");
    }
}
