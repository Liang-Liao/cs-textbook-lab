/* Demo: maximum flow Edmonds-Karp (CLRS Ch.26). */
#include <stdio.h>

#include "maxflow.h"

int main(void) {
  FlowGraph g;
  fg_init(&g, 6);
  /* CLRS Fig 26.1 */
  fg_set_capacity(&g, 0, 1, 16);
  fg_set_capacity(&g, 0, 2, 13);
  fg_set_capacity(&g, 1, 2, 10);
  fg_set_capacity(&g, 1, 3, 12);
  fg_set_capacity(&g, 2, 1, 4);
  fg_set_capacity(&g, 2, 4, 14);
  fg_set_capacity(&g, 3, 2, 9);
  fg_set_capacity(&g, 3, 5, 20);
  fg_set_capacity(&g, 4, 3, 7);
  fg_set_capacity(&g, 4, 5, 4);

  long long f = max_flow_edmonds_karp(&g, 0, 5);
  printf("=== CLRS Fig 26.1 max flow = %lld (book: 23) ===\n", f);

  printf("net flow on edge pairs:\n");
  int edges[][3] = {{0, 1, 16}, {0, 2, 13}, {1, 2, 10}, {1, 3, 12},
                    {2, 1, 4},  {2, 4, 14}, {3, 2, 9},  {3, 5, 20},
                    {4, 3, 7},  {4, 5, 4}};
  for (int i = 0; i < 10; i++) {
    int u = edges[i][0], v = edges[i][1], cap = edges[i][2];
    printf("  %d->%d  cap=%d netflow=%lld\n", u, v, cap,
           fg_flow_on(&g, u, v));
  }

  fg_destroy(&g);
  return 0;
}
