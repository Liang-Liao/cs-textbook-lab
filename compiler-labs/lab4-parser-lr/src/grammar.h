/*
 * grammar.h —— MiniC v3 文法（为 SLR(1) 准备，对照龙书 4.5~4.6 节）
 *
 * 关键对照：这是**原始的左递归文法**——LR 系分析器直接吃左递归，
 * 不需要 lab3 那套"消除左递归"的变换！左递归天然表达左结合，
 * 所以这个文法比 lab3 的 LL(1) 文法更接近我们"想要表达的东西"。
 *
 *   S' → stmt_list                    （增广开始符号，识别"接受"用）
 *   stmt_list → stmt_list stmt | ε
 *   stmt → print expr ;
 *        | expr ;
 *        | if ( expr ) stmt           ← 故意保留悬空 else
 *        | if ( expr ) stmt else stmt ← 产生 shift/reduce 冲突（本 lab 主秀）
 *        | { stmt_list }
 *        | ;
 *   expr → expr == rel | expr != rel | rel
 *   rel  → rel < add | rel <= add | rel > add | rel >= add | add
 *   add  → add + term | add - term | term
 *   term → term * factor | term / factor | term % factor | factor
 *   factor → ( expr ) | - factor | INT | FLOAT
 */
#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdint.h>
#include "token.h"

#define EPS      (-2)
#define NT_BASE  100

enum NonTerm {
    NT_S_PRIME,     /* S'（增广） */
    NT_STMT_LIST,
    NT_STMT,
    NT_EXPR,
    NT_REL,
    NT_ADD,
    NT_TERM,
    NT_FACTOR,
    NT_COUNT
};

typedef struct {
    int lhs;
    int rhs[8];
    int n;
} Prod;

extern Prod PRODS[];
extern int  NPRODS;

const char *sym_name(int sym);

/* FOLLOW 集（SLR 用它决定"在哪些 lookahead 下可以归约"） */
extern uint64_t FOLLOW_SET[NT_COUNT];
void compute_follow(void);

/* 便捷判断：终结符 t 在非终结符 nt 的 FOLLOW 里吗 */
static inline int in_follow(int nt, int t)
{
    return (int)(FOLLOW_SET[nt] >> t & 1);
}

void print_grammar(void);

#endif /* GRAMMAR_H */
