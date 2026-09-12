/*
 * nfa.h —— Thompson 构造：正则 AST → ε-NFA（lab2，龙书 3.7 节）
 */
#ifndef NFA_H
#define NFA_H

#include "re_parse.h"

/* NFA 边的种类 */
typedef enum { E_EPS, E_CH, E_CLASS, E_ANY } EKind;

/* 一条边：要么是 ε，要么消费一个字符（精确字符/字符类/任意字符） */
typedef struct {
    EKind kind;
    int   ch;                   /* E_CH */
    const unsigned char *cls;   /* E_CLASS（指向 AST 节点里的位图） */
    int   neg;                  /* E_CLASS 的取反标志 */
    int   to;                   /* 目标状态 */
} Edge;

/* Thompson NFA 的状态：出边最多 2 条（这是 Thompson 构造的标志性约束，
 * 让子集构造的实现非常干净）。tag != 0 表示接受状态，
 * tag 记录"是哪条规则接受的"（mini-flex 用它区分记号类别）。 */
typedef struct {
    Edge e[2];
    int  nedge;
    int  tag;
} NState;

typedef struct {
    NState *st;
    int     n, cap;
    int     start;
    int     accept;   /* 单一模式时唯一的接受状态（构建时返回的碎片用） */
} NFA;

void nfa_init(NFA *nfa);
int   nfa_new_state(NFA *nfa);                    /* 新建状态，返回编号 */
void  nfa_edge(NFA *nfa, int from, EKind k, int ch,
               const unsigned char *cls, int neg, int to);
void  nfa_build(NFA *nfa, Node *ast);             /* 单一模式：正则 → NFA */
void  nfa_build_multi(NFA *nfa, Node **asts, int nrules); /* 多模式并联 */
void  nfa_dump_dot(const NFA *nfa, const char *title);    /* DOT 图输出 */

#endif /* NFA_H */
