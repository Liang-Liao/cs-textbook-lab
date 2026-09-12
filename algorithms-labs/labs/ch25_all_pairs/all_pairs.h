#ifndef CLRS_ALL_PAIRS_H
#define CLRS_ALL_PAIRS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.25 All-pairs shortest paths.
 * Adjacency-matrix representation; W[i][j] = weight or INF (no edge).
 */

#define APSP_INF (1LL << 60)

/*
 * CLRS 25.2 Floyd-Warshall.
 * n vertices; W is n*n row-major. dist_out is n*n.
 * Returns 0 if a negative-weight cycle is detected.
 */
int floyd_warshall(const long long *W, size_t n, long long *dist_out);

/*
 * CLRS 25.3 Johnson — Bellman-Ford reweight + Dijkstra per source.
 * W is n*n adjacency matrix (may have negative edges, no neg cycle).
 * dist_out is n*n. Returns 0 if negative cycle exists.
 */
int johnson(const long long *W, size_t n, long long *dist_out);

/* Build n x n matrix with APSP_INF on missing edges (set diag 0). */
void apsp_init_matrix(long long *W, size_t n);

/* Set edge u->v = w (overwrites). */
void apsp_set_edge(long long *W, size_t n, size_t u, size_t v, long long w);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_ALL_PAIRS_H */
