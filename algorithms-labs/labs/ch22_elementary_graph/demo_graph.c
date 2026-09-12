/* Demo: BFS, DFS topo, SCC (CLRS Ch.22). */
#include <stdio.h>

#include "graph.h"

int main(void) {
  printf("=== CLRS 22.2 BFS ===\n");
  Digraph g;
  graph_init(&g, 6);
  int edges[][2] = {{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 5}, {2, 5}};
  for (int i = 0; i < 8; i++) {
    graph_add_edge(&g, edges[i][0], edges[i][1]);
    graph_add_edge(&g, edges[i][1], edges[i][0]);
  }
  int parent[6], dist[6];
  bfs(&g, 0, parent, dist);
  printf("BFS from 0: dist =");
  for (int i = 0; i < 6; i++) {
    printf(" %d", dist[i]);
  }
  printf("\n");
  graph_destroy(&g);

  printf("\n=== CLRS 22.4 Topological sort ===\n");
  graph_init(&g, 6);
  graph_add_edge(&g, 5, 2);
  graph_add_edge(&g, 5, 0);
  graph_add_edge(&g, 4, 0);
  graph_add_edge(&g, 4, 1);
  graph_add_edge(&g, 2, 3);
  graph_add_edge(&g, 3, 1);
  int order[6];
  topo_sort(&g, order);
  printf("topo order:");
  for (int i = 0; i < 6; i++) {
    printf(" %d", order[i]);
  }
  printf("\n");
  graph_destroy(&g);

  printf("\n=== CLRS 22.5 Strongly connected components ===\n");
  graph_init(&g, 8);
  int scc_e[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 2}, {3, 4}, {4, 5}, {5, 4},
                    {5, 6}, {6, 6}, {6, 7}, {7, 6}, {7, 5}, {1, 5}, {1, 7},
                    {0, 4}};
  for (int i = 0; i < 15; i++) {
    graph_add_edge(&g, scc_e[i][0], scc_e[i][1]);
  }
  int comp[8];
  int k = scc_kosaraju(&g, comp);
  printf("%d SCCs; comp[v] =", k);
  for (int i = 0; i < 8; i++) {
    printf(" %d", comp[i]);
  }
  printf("\n");
  graph_destroy(&g);

  return 0;
}
