/* Demo: MST Kruskal and Prim (CLRS Ch.23). */
#include <stdio.h>

#include "mst.h"

int main(void) {
  WGraph g;
  wgraph_init(&g, 9, 16);
  wgraph_add_edge(&g, 0, 1, 4);
  wgraph_add_edge(&g, 0, 7, 8);
  wgraph_add_edge(&g, 1, 2, 8);
  wgraph_add_edge(&g, 1, 7, 11);
  wgraph_add_edge(&g, 2, 3, 7);
  wgraph_add_edge(&g, 2, 5, 4);
  wgraph_add_edge(&g, 2, 8, 2);
  wgraph_add_edge(&g, 3, 4, 9);
  wgraph_add_edge(&g, 3, 5, 14);
  wgraph_add_edge(&g, 4, 5, 10);
  wgraph_add_edge(&g, 5, 6, 2);
  wgraph_add_edge(&g, 6, 7, 1);
  wgraph_add_edge(&g, 6, 8, 6);
  wgraph_add_edge(&g, 7, 8, 7);

  printf("=== CLRS Fig 23.1 MST (weight should be 37) ===\n");

  WEdge mst[16];
  size_t k = mst_kruskal(&g, mst);
  printf("Kruskal %zu edges, total=%lld\n  edges:", k, mst_weight(mst, k));
  for (size_t i = 0; i < k; i++) {
    printf(" (%d-%d w=%d)", mst[i].u, mst[i].v, mst[i].w);
  }
  printf("\n");

  k = mst_prim(&g, 0, mst);
  printf("Prim(0) %zu edges, total=%lld\n  edges:", k, mst_weight(mst, k));
  for (size_t i = 0; i < k; i++) {
    printf(" (%d-%d w=%d)", mst[i].u, mst[i].v, mst[i].w);
  }
  printf("\n");

  wgraph_destroy(&g);
  return 0;
}
