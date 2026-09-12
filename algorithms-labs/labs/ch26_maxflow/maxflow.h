#ifndef CLRS_MAXFLOW_H
#define CLRS_MAXFLOW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.26 Maximum flow — directed graph, capacities on edges.
 * Vertices 0..V-1; source s, sink t.
 */

typedef struct {
  int V;
  long long *cap; /* V*V residual capacity matrix (row-major); holds the
                   * original capacities until max flow runs, then the
                   * final residual capacities */
  long long *flow; /* V*V net-flow matrix (row-major) */
} FlowGraph;

void fg_init(FlowGraph *g, int V);
void fg_destroy(FlowGraph *g);

/* Set/overwrite capacity u->v. Call before running max flow. */
void fg_set_capacity(FlowGraph *g, int u, int v, long long cap);

/*
 * CLRS 26.2 Ford-Fulkerson with BFS (Edmonds-Karp).
 * Returns the max flow value. Afterwards g->cap holds the residual
 * capacities and fg_flow_on(g,u,v) reports the net flow of pair (u,v).
 */
long long max_flow_edmonds_karp(FlowGraph *g, int s, int t);

/* Net flow across the pair (u, v): positive = flow u->v, negative = net
 * flow v->u. For graphs without antiparallel edges this equals the flow
 * on the single edge (u,v). */
long long fg_flow_on(const FlowGraph *g, int u, int v);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MAXFLOW_H */
