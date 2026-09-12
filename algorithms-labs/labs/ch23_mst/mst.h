#ifndef CLRS_MST_H
#define CLRS_MST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.23 Minimum spanning trees — undirected weighted graph.
 */

typedef struct {
  int u, v;
  int w;
} WEdge;

typedef struct {
  int V;
  size_t E;
  size_t max_edges; /* capacity allocated for edges */
  WEdge *edges; /* undirected: each edge listed once */
} WGraph;

void wgraph_init(WGraph *g, int V, size_t max_edges);
void wgraph_destroy(WGraph *g);
void wgraph_add_edge(WGraph *g, int u, int v, int w);

/* Result MST as edge list; caller provides out[max_edges]. Returns edge count. */
size_t mst_kruskal(const WGraph *g, WEdge *out);
size_t mst_prim(const WGraph *g, int r, WEdge *out);

/* Total weight of an edge list. */
long long mst_weight(const WEdge *edges, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MST_H */
