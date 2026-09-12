/*
 * nfa.c —— Thompson 构造实现（对照龙书 3.7 节，McNaughton-Yamida-Thompson）
 *
 * 每个构造步骤（对照龙书图 3.x 的基本 NFA 片段）都只在现有片段上
 * 增加 O(1) 个状态和 O(1) 条 ε 边，这是 Thompson 构造最优美的地方：
 *
 *   字符 c:   (s) --c--> (f)                 2 状态
 *   r|s  :    新开始 --ε--> r的开始, s的开始；r、s 的接受 --ε--> 新接受
 *   rs   :    r 的接受 --ε--> s 的开始
 *   r*   :    新开始 --ε--> r开始/新接受；r接受 --ε--> r开始/新接受
 *   r+   :    同 r* 但删掉"新开始 --ε--> 新接受"的直达边
 *   r?   :    新开始 --ε--> r开始/新接受；r接受 --ε--> 新接受
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nfa.h"

void nfa_init(NFA *nfa)
{
    nfa->cap = 64;
    nfa->st = malloc(sizeof(NState) * (size_t)nfa->cap);
    nfa->n = 0;
    nfa->start = nfa->accept = -1;
}

int nfa_new_state(NFA *nfa)
{
    if (nfa->n == nfa->cap) {
        nfa->cap *= 2;
        nfa->st = realloc(nfa->st, sizeof(NState) * (size_t)nfa->cap);
    }
    NState *s = &nfa->st[nfa->n];
    s->nedge = 0;
    s->tag = 0;
    return nfa->n++;
}

void nfa_edge(NFA *nfa, int from, EKind k, int ch,
              const unsigned char *cls, int neg, int to)
{
    NState *s = &nfa->st[from];
    Edge *e = &s->e[s->nedge++];
    e->kind = k;
    e->ch = ch;
    e->cls = cls;
    e->neg = neg;
    e->to = to;
}

/* 片段：一段子 NFA 的入口和出口（龙书叫"开始/接受状态对"） */
typedef struct { int start, accept; } Frag;

static Frag build(NFA *nfa, Node *n, int tag)
{
    switch (n->kind) {
    case N_EMPTY: {                       /* ε：s --ε--> f */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        nfa_edge(nfa, s, E_EPS, 0, NULL, 0, f);
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    case N_CHAR: {                        /* s --c--> f */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        nfa_edge(nfa, s, E_CH, n->ch, NULL, 0, f);
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    case N_CLASS: case N_ANY: {           /* s --类/任意--> f */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        nfa_edge(nfa, s, n->kind == N_CLASS ? E_CLASS : E_ANY,
                 0, n->cls, n->neg, f);
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    case N_CAT: {                         /* r 的接受 --ε--> s 的开始 */
        Frag a = build(nfa, n->l, 0);
        Frag b = build(nfa, n->r, tag);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, b.start);
        nfa->st[a.accept].tag = 0; /* 现在只是中转，不再是接受状态 */
        return (Frag){a.start, b.accept};
    }
    case N_ALT: {                         /* 新 s --ε--> 两个分支；汇到新 f */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        Frag a = build(nfa, n->l, 0);
        Frag b = build(nfa, n->r, 0);
        nfa_edge(nfa, s, E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, s, E_EPS, 0, NULL, 0, b.start);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, f);
        nfa_edge(nfa, b.accept, E_EPS, 0, NULL, 0, f);
        nfa->st[f].tag = tag;             /* tag 传给公共接受态 */
        return (Frag){s, f};
    }
    case N_STAR: {                        /* 见文件头注释的 r* 图 */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        Frag a = build(nfa, n->l, 0);
        nfa_edge(nfa, s,      E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, s,      E_EPS, 0, NULL, 0, f);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, f);
        nfa->st[a.accept].tag = 0;
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    case N_PLUS: {                        /* r+ = r 再绕回，但不许空过 */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        Frag a = build(nfa, n->l, 0);
        nfa_edge(nfa, s,        E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, f);
        nfa->st[a.accept].tag = 0;
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    case N_QUEST: {                       /* r? = 可以空过 */
        int s = nfa_new_state(nfa), f = nfa_new_state(nfa);
        Frag a = build(nfa, n->l, 0);
        nfa_edge(nfa, s,        E_EPS, 0, NULL, 0, a.start);
        nfa_edge(nfa, s,        E_EPS, 0, NULL, 0, f);
        nfa_edge(nfa, a.accept, E_EPS, 0, NULL, 0, f);
        nfa->st[a.accept].tag = 0;
        nfa->st[f].tag = tag;
        return (Frag){s, f};
    }
    }
    /* 不可达 */
    fprintf(stderr, "internal error: bad node kind\n");
    exit(1);
}

void nfa_build(NFA *nfa, Node *ast)
{
    Frag f = build(nfa, ast, 1); /* 单一模式 tag=1 */
    nfa->start = f.start;
    nfa->accept = f.accept;
}

/* mini-flex 用：多条规则并联。为了不破坏"每个状态最多 2 条出边"的
 * Thompson 不变量（见 NState 定义），把 39 条 ε 边改成一条链：
 *
 *   s0 --ε--> 规则0开始    s0 --ε--> s1 --ε--> 规则1开始    s1 --ε--> s2 ...
 *
 * 每个链上状态恰好 2 条 ε 边，效果与"公共开始状态"完全等价。
 * 规则 i 的接受状态 tag = i+1（规则编号，越小优先级越高）。 */
void nfa_build_multi(NFA *nfa, Node **asts, int nrules)
{
    int prev = nfa_new_state(nfa);
    nfa->start = prev;
    for (int i = 0; i < nrules; i++) {
        Frag f = build(nfa, asts[i], i + 1);
        nfa_edge(nfa, prev, E_EPS, 0, NULL, 0, f.start);
        if (i + 1 < nrules) {
            int next = nfa_new_state(nfa);
            nfa_edge(nfa, prev, E_EPS, 0, NULL, 0, next);
            prev = next;
        }
    }
}

/* 边的标签文本（DOT 输出用） */
static void edge_label(char *buf, const Edge *e)
{
    switch (e->kind) {
    case E_EPS:   strcpy(buf, "ε");  break;
    case E_CH:
        if (e->ch >= 32 && e->ch < 127) sprintf(buf, "'%c'", e->ch);
        else sprintf(buf, "\\x%02x", e->ch);
        break;
    case E_CLASS: strcpy(buf, e->neg ? "[^类]" : "[类]"); break;
    case E_ANY:   strcpy(buf, "."); break;
    }
}

/* 输出 graphviz DOT。用法: ./minire.exe -dot '(a|b)*c' | dot -Tsvg -o nfa.svg */
void nfa_dump_dot(const NFA *nfa, const char *title)
{
    printf("digraph \"%s\" {\n  rankdir=LR;\n  node [shape=circle];\n", title);
    printf("  \"%d\" [shape=doublecircle];\n", nfa->start); /* 起点双圈便于辨认 */
    for (int i = 0; i < nfa->n; i++) {
        if (nfa->st[i].tag)
            printf("  %d [shape=doublecircle];\n", i);
        for (int j = 0; j < nfa->st[i].nedge; j++) {
            char lb[16];
            edge_label(lb, &nfa->st[i].e[j]);
            printf("  %d -> %d [label=\"%s\"];\n",
                   i, nfa->st[i].e[j].to, lb);
        }
    }
    printf("}\n");
}
