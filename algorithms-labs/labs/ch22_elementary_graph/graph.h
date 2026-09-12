#ifndef CLRS_GRAPH_H
#define CLRS_GRAPH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.22 Directed graph as adjacency lists.
 * Vertices are 0..V-1.
 */
typedef struct EdgeNode {
  int v;
  struct EdgeNode *next;
} EdgeNode;

typedef struct {
  int V;
  EdgeNode **adj; /* head of out-edge list */
  size_t E;
} Digraph;

void graph_init(Digraph *g, int V);
void graph_destroy(Digraph *g);
void graph_add_edge(Digraph *g, int u, int v);

/* Transpose (reverse all edges); caller destroys. */
void graph_transpose(const Digraph *g, Digraph *out);

/* CLRS 22.2 BFS. parent[] length V (-1 = none); dist[] = d(s,v) or -1. */
void bfs(const Digraph *g, int s, int *parent, int *dist);

/* CLRS 22.3 DFS over all vertices. disc[]/fin[] 1-based times, 0=undiscovered.
 * If fin_order[] is non-NULL it receives vertices in finish-time
 * INCREASING order (reverse it before the second DFS of Kosaraju SCC). */
void dfs(const Digraph *g, int *disc, int *fin, int *fin_order);

/* CLRS 22.4 Topological sort. order[] receives vertices in topo order.
 * Returns 1 on success, 0 if a cycle exists. */
int topo_sort(const Digraph *g, int *order);

/* CLRS 22.5 Strongly connected components (Kosaraju).
 * comp[v] in 0..k-1; returns k. Components numbered in reverse topo of G^T
 * (sink components get smaller ids). */
int scc_kosaraju(const Digraph *g, int *comp);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_GRAPH_H */
