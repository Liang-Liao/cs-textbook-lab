/*
 * dfa.c —— 子集构造与最小化的实现（lab2 核心）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dfa.h"

/* ============ 位集合：DFA 的一个状态 = NFA 状态的一个子集 ============ */
#define MAXNFA  2048                          /* 支持 NFA 最多 2048 状态 */
#define NWORDS ((MAXNFA + 63) / 64)

typedef struct { uint64_t b[NWORDS]; } Set;

static void set_add(Set *s, int i)      { s->b[i >> 6] |= 1ull << (i & 63); }
static int  set_has(const Set *s, int i){ return (int)(s->b[i >> 6] >> (i & 63) & 1); }
static int  set_empty(const Set *s)
{
    for (int i = 0; i < NWORDS; i++)
        if (s->b[i]) return 0;
    return 1;
}
static int  set_equal(const Set *a, const Set *b)
{
    for (int i = 0; i < NWORDS; i++)
        if (a->b[i] != b->b[i]) return 0;
    return 1;
}

/* ε 闭包：从集合 S 只走 ε 边能到的所有状态（含 S 自身）。
 * 龙书 3.7 节的 ε-closure，子集构造的基石。用显式栈做图遍历。 */
static void eps_closure(const NFA *nfa, Set *s)
{
    int stack[2048], top = 0;
    for (int i = 0; i < nfa->n; i++)
        if (set_has(s, i)) stack[top++] = i;

    while (top > 0) {
        int u = stack[--top];
        const NState *st = &nfa->st[u];
        for (int j = 0; j < st->nedge; j++) {
            if (st->e[j].kind == E_EPS) {
                int v = st->e[j].to;
                if (!set_has(s, v)) {
                    set_add(s, v);
                    stack[top++] = v; /* 新加入的还要继续闭包 */
                }
            }
        }
    }
}

/* 边是否消费字符 c（ε 不消费；类要看位图） */
static int edge_matches(const Edge *e, unsigned char c)
{
    switch (e->kind) {
    case E_EPS:    return 0;
    case E_CH:     return e->ch == c;
    case E_CLASS:  return (int)((e->cls[c >> 3] >> (c & 7)) & 1) ^ e->neg;
    case E_ANY:    return c != '\n';
    }
    return 0;
}

/* ============ 子集构造（龙书算法 3.20 的直译） ============
 * 思想：DFA 的一个状态 = "NFA 读入同样字符串后可能处于的所有状态"。
 * Dstates 是未标记/已标记的子集集合；每取一个未标记子集 T，
 * 对每个输入字符 c 求 move(T,c) 的 ε 闭包，作为转移目标。 */
DFA dfa_from_nfa(const NFA *nfa)
{
    if (nfa->n > MAXNFA) {
        fprintf(stderr, "internal error: NFA 状态数 %d 超过上限 %d\n",
                nfa->n, MAXNFA);
        exit(1);
    }
    int cap = 256;
    Set *states = malloc(sizeof(Set) * (size_t)cap); /* 每个已发现 DFA 状态的子集 */
    int  n = 0;

    DFA d = {0};
    d.trans = malloc(sizeof(int) * (size_t)(cap * 256));
    d.acc   = calloc((size_t)cap, sizeof(int));

    /* 开始状态 = ε-closure({nfa->start}) */
    Set s0 = {0};
    set_add(&s0, nfa->start);
    eps_closure(nfa, &s0);
    states[n++] = s0;

    /* BFS 工作表（龙书:"唯一未被标记的状态"），数组即队列 */
    for (int cur = 0; cur < n; cur++) {
        for (int c = 0; c < 256; c++) {
            /* move：从 T 沿"消费 c 的边"走一步 */
            Set t = {0};
            for (int i = 0; i < nfa->n; i++) {
                if (!set_has(&states[cur], i)) continue;
                const NState *st = &nfa->st[i];
                for (int j = 0; j < st->nedge; j++)
                    if (edge_matches(&st->e[j], (unsigned char)c))
                        set_add(&t, st->e[j].to);
            }
            if (set_empty(&t)) {
                d.trans[cur * 256 + c] = -1;  /* 死路 */
                continue;
            }
            eps_closure(nfa, &t);

            /* 子集是否已存在？线性查找（flex 用哈希；这里状态少，足够） */
            int found = -1;
            for (int k = 0; k < n; k++)
                if (set_equal(&states[k], &t)) { found = k; break; }

            if (found < 0) {
                if (n == cap) { /* 扩容 */
                    cap *= 2;
                    states = realloc(states, sizeof(Set) * (size_t)cap);
                    d.trans = realloc(d.trans, sizeof(int) * (size_t)(cap * 256));
                    d.acc   = realloc(d.acc, sizeof(int) * (size_t)cap);
                    memset(d.acc + n, 0, sizeof(int) * (size_t)(cap - n));
                }
                states[n] = t;
                found = n++;
            }
            d.trans[cur * 256 + c] = found;
        }
        /* 接受标记：子集中含接受 NFA 状态 → 取最小 tag（规则优先级） */
        for (int i = 0; i < nfa->n; i++)
            if (set_has(&states[cur], i) && nfa->st[i].tag) {
                int tag = nfa->st[i].tag;
                if (d.acc[cur] == 0 || tag < d.acc[cur])
                    d.acc[cur] = tag;
            }
    }

    d.n = n;
    d.start = 0;
    free(states);
    return d;
}

/* ============ 最小化：划分细化 ============
 * 等价状态定义：接受标记相同，且对每个字符都转移到等价状态。
 * 做法：初始划分 = 按接受标记分组；每轮把组内"转移签名"（对每个字符
 * 去往的组）相同的状态聚成一个子组，签名不同的才分裂；直到不动点。
 * 注意必须**同签名聚拢**而不是各自成新组——否则两个互相等价、但都与
 * 组代表不等价的状态会被永久拆散，得到非最小 DFA。 */
DFA dfa_minimize(const DFA *d)
{
    int n = d->n;
    int *grp = malloc(sizeof(int) * (size_t)n);  /* 状态 -> 组号 */

    /* 初始分组：非接受=组0；接受按 tag 各一组（tag 不同的不能合并，
     * 否则分不清输出哪条规则的记号！） */
    int ngrp = 1;
    for (int i = 0; i < n; i++) {
        if (d->acc[i] == 0) grp[i] = 0;
        else {
            int g = -1;
            for (int j = 0; j < i; j++)
                if (d->acc[j] == d->acc[i]) { g = grp[j]; break; }
            if (g < 0) g = ngrp++;
            grp[i] = g;
        }
    }

    /* 反复细化直到不动点。每轮以旧划分为基准重新聚类：状态 i 并入
     * 本轮内第一个"同旧组、同标记、转移签名一致"的领袖状态所在的新
     * 组；没有就自当领袖开新组。组数不再变化即到达不动点。 */
    int changed = 1;
    while (changed) {
        changed = 0;
        int *tent = malloc(sizeof(int) * (size_t)n);   /* 本轮产生的新组号 */
        int *lead = malloc(sizeof(int) * (size_t)n);   /* 各新组的领袖状态 */
        int nl = 0, ng = 0;
        for (int i = 0; i < n; i++) {
            int gid = -1;
            for (int k = 0; k < nl; k++) {
                int l = lead[k];
                if (grp[l] != grp[i] || d->acc[l] != d->acc[i]) continue;
                int same = 1;
                for (int c = 0; c < 256 && same; c++) {
                    int ta = d->trans[l * 256 + c], tb = d->trans[i * 256 + c];
                    int ga = ta < 0 ? -1 : grp[ta];
                    int gb = tb < 0 ? -1 : grp[tb];
                    if (ga != gb) same = 0;
                }
                if (same) { gid = tent[l]; break; }   /* 与领袖等价 → 同组 */
            }
            if (gid < 0) { gid = ng++; lead[nl++] = i; }
            tent[i] = gid;
        }
        memcpy(grp, tent, sizeof(int) * (size_t)n);
        free(tent); free(lead);
        if (ng != ngrp) { ngrp = ng; changed = 1; }
    }

    /* 按组重建紧凑 DFA */
    DFA m = {0};
    m.n = ngrp;
    m.start = grp[d->start];
    m.trans = malloc(sizeof(int) * (size_t)(ngrp * 256));
    m.acc   = calloc((size_t)ngrp, sizeof(int));

    int *done = calloc((size_t)ngrp, sizeof(int));
    for (int i = 0; i < n; i++) {
        if (done[grp[i]]) continue;
        done[grp[i]] = 1;
        m.acc[grp[i]] = d->acc[i];
        for (int c = 0; c < 256; c++) {
            int t = d->trans[i * 256 + c];
            m.trans[grp[i] * 256 + c] = t < 0 ? -1 : grp[t];
        }
    }
    free(grp); free(done);
    return m;
}

/* 整串匹配：从 start 走到底，要求终点是接受状态 */
int dfa_accept_all(const DFA *d, const char *s)
{
    int st = d->start;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        st = d->trans[st * 256 + *p];
        if (st < 0) return 0;
    }
    return d->acc[st] != 0;
}

void dfa_dump_dot(const DFA *d, const char *title)
{
    printf("digraph \"%s\" {\n  rankdir=LR;\n  node [shape=circle];\n", title);
    printf("  start [shape=point];\n  start -> %d;\n", d->start);
    for (int i = 0; i < d->n; i++) {
        if (d->acc[i])
            printf("  %d [shape=doublecircle,label=\"%d/规则%d\"];\n",
                   i, i, d->acc[i]);
        /* 把 256 列转移压缩成区间显示，否则图没法看 */
        for (int c = 0; c < 256; c++) {
            int t = d->trans[i * 256 + c];
            if (t < 0) continue;
            int c2 = c;
            while (c2 + 1 < 256 && d->trans[i * 256 + c2 + 1] == t) c2++;
            char lb[32];
            if (c == c2)
                snprintf(lb, sizeof lb,
                         c >= 32 && c < 127 ? "%c" : "\\x%02x", c);
            else
                snprintf(lb, sizeof lb, "%x-%x", c, c2);
            printf("  %d -> %d [label=\"%s\"];\n", i, t, lb);
            c = c2;
        }
    }
    printf("}\n");
}
