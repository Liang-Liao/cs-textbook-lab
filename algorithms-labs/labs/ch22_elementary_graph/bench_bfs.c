/* Bench: BFS on random sparse digraph (CLRS Ch.22). */
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "graph.h"
#include "timing.h"

int main(void) {
  printf("=== BFS timing (random digraph) ===\n");
  const int sizes[] = {1000, 4000, 8000};
  for (int si = 0; si < 3; si++) {
    int V = sizes[si];
    Digraph g;
    graph_init(&g, V);
    srand((unsigned)V);
    int edges = V * 4;
    for (int e = 0; e < edges; e++) {
      int u = rand() % V;
      int v = rand() % V;
      if (u != v) {
        graph_add_edge(&g, u, v);
      }
    }
    int *parent = clrs_xmalloc((size_t)V * sizeof(int));
    int *dist = clrs_xmalloc((size_t)V * sizeof(int));
    int64_t t0 = clrs_now_us();
    bfs(&g, 0, parent, dist);
    int64_t dt = clrs_elapsed_us(t0);
    printf("  V=%5d E~%5d  BFS %8lld us\n", V, edges, (long long)dt);
    free(parent);
    free(dist);
    graph_destroy(&g);
  }
  return 0;
}
