/* Demo: single-source shortest paths (CLRS Ch.24). */
#include <limits.h>
#include <stdio.h>

#include "shortest_paths.h"

static void print_dist(const char *label, const long long *dist, int n) {
  printf("%s:", label);
  for (int i = 0; i < n; i++) {
    if (dist[i] == LLONG_MAX) {
      printf(" INF");
    } else {
      printf(" %lld", dist[i]);
    }
  }
  printf("\n");
}

int main(void) {
  SPGraph g;
  spgraph_init(&g, 5, 16);
  /* CLRS Fig 24.6 */
  spgraph_add_edge(&g, 0, 1, 10);
  spgraph_add_edge(&g, 0, 3, 5);
  spgraph_add_edge(&g, 1, 2, 1);
  spgraph_add_edge(&g, 1, 3, 2);
  spgraph_add_edge(&g, 2, 1, 4);
  spgraph_add_edge(&g, 2, 4, 4);
  spgraph_add_edge(&g, 3, 1, 3);
  spgraph_add_edge(&g, 3, 2, 9);
  spgraph_add_edge(&g, 3, 4, 2);
  spgraph_add_edge(&g, 4, 0, 7);
  spgraph_add_edge(&g, 4, 2, 6);

  long long dist[5];
  int parent[5];

  printf("=== CLRS Fig 24.6 from s=0 ===\n");
  bellman_ford(&g, 0, dist, parent);
  print_dist("Bellman-Ford", dist, 5);
  dijkstra(&g, 0, dist, parent);
  print_dist("Dijkstra     ", dist, 5);

  int path[8];
  size_t pn = sp_path(0, 4, parent, path, 8);
  printf("path s->z:");
  for (size_t i = 0; i < pn; i++) {
    printf(" %d", path[i]);
  }
  printf("\n");

  spgraph_destroy(&g);
  return 0;
}
