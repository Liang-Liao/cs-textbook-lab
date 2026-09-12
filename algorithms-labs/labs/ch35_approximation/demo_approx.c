/* Demo: approximation algorithms (CLRS Ch.35). */
#include <math.h>
#include <stdio.h>

#include "approx.h"

int main(void) {
  printf("=== CLRS 35.1 Vertex cover 2-approx ===\n");
  int eu[] = {0, 0, 0, 1, 2};
  int ev[] = {1, 2, 3, 2, 3};
  int cover[8];
  size_t c = vertex_cover_approx(4, eu, ev, 5, cover, 8);
  printf("cover size %zu:", c);
  for (size_t i = 0; i < c; i++) {
    printf(" %d", cover[i]);
  }
  printf("\n");

  printf("\n=== CLRS 35.2 Metric TSP 2-approx ===\n");
  double pts[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  double d[16];
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      double dx = pts[i][0] - pts[j][0];
      double dy = pts[i][1] - pts[j][1];
      d[i * 4 + j] = sqrt(dx * dx + dy * dy);
    }
  }
  int tour[8];
  double cost = tsp_metric_approx(d, 4, tour);
  printf("tour:");
  for (int i = 0; i < 4; i++) {
    printf(" %d", tour[i]);
  }
  printf(" cost=%.4f (opt ~4)\n", cost);
  return 0;
}
