#ifndef CLRS_SHORTEST_PATHS_H
#define CLRS_SHORTEST_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.24 Single-source shortest paths.
 * Directed weighted graph; vertices 0..V-1.
 */

typedef struct {
  int u, v;
  int w;
} SPEdge;

typedef struct {
  int V;
  size_t E;
  size_t max_edges; /* capacity allocated for edges */
  SPEdge *edges;
} SPGraph;

void spgraph_init(SPGraph *g, int V, size_t max_edges);
void spgraph_destroy(SPGraph *g);
void spgraph_add_edge(SPGraph *g, int u, int v, int w);

/*
 * CLRS 24.1 Bellman-Ford.
 * dist[v] = shortest path weight from s, or LLONG_MAX if unreachable.
 * parent[v] = predecessor or -1.
 * Returns 1 if no negative-weight cycle reachable from s, 0 otherwise.
 */
int bellman_ford(const SPGraph *g, int s, long long *dist, int *parent);

/*
 * CLRS 24.2 DAG shortest paths.
 * Uses a given topological order (length V); dist/parent as above.
 * Assumes no positive cycles (DAG has none). Unreachable = LLONG_MAX.
 */
void dag_shortest_paths(const SPGraph *g, const int *topo, int s,
                        long long *dist, int *parent);

/*
 * CLRS 24.3 Dijkstra (requires non-negative weights).
 * O(V^2) array implementation. Unreachable = LLONG_MAX.
 * Returns 0 if a negative weight is seen.
 */
int dijkstra(const SPGraph *g, int s, long long *dist, int *parent);

/* Reconstruct path s -> v into path_out[] (s first, v last). Returns the
 * number of vertices written; 0 if v is unreachable from s or the path
 * does not fit max_cap. */
size_t sp_path(int s, int v, const int *parent, int *path_out,
               size_t max_cap);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_SHORTEST_PATHS_H */
