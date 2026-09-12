#ifndef CLRS_APPROX_H
#define CLRS_APPROX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.35 Approximation algorithms.
 */

/*
 * CLRS 35.1 Vertex cover: 2-approximation.
 * Undirected graph, edges as pairs (eu[i], ev[i]), m edges, n vertices.
 * cover_out receives selected vertices; returns size of cover.
 * Greedy: repeatedly pick an edge, take both endpoints, remove incident edges.
 */
size_t vertex_cover_approx(size_t n, const int *eu, const int *ev, size_t m,
                           int *cover_out, size_t max_cover);

/*
 * CLRS 35.2 Metric TSP 2-approximation (double MST / preorder).
 * Dist matrix n*n, metric (non-negative, triangle inequality).
 * tour_out receives a cycle of n vertices starting at 0 (permutation of
 * all vertices); returns tour cost. Returns -1 if n < 1.
 * Uses MST + preorder walk shortcutting; only assumes symmetry for cost.
 */
double tsp_metric_approx(const double *dist, size_t n, int *tour_out);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_APPROX_H */
