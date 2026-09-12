/* Demo: all-pairs shortest paths (CLRS Ch.25). */
#include <stdio.h>

#include "all_pairs.h"

static void print_mat(const char *label, const long long *D, size_t n) {
  printf("%s:\n", label);
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      if (D[i * n + j] == APSP_INF) {
        printf(" INF");
      } else {
        printf(" %3lld", D[i * n + j]);
      }
    }
    printf("\n");
  }
}

int main(void) {
  long long W[25];
  apsp_init_matrix(W, 5);
  /* CLRS Fig 25.1 */
  apsp_set_edge(W, 5, 0, 1, 3);
  apsp_set_edge(W, 5, 0, 4, -4);
  apsp_set_edge(W, 5, 0, 2, 8);
  apsp_set_edge(W, 5, 1, 3, 1);
  apsp_set_edge(W, 5, 1, 4, 7);
  apsp_set_edge(W, 5, 2, 1, 4);
  apsp_set_edge(W, 5, 3, 0, 2);
  apsp_set_edge(W, 5, 3, 2, -5);
  apsp_set_edge(W, 5, 4, 3, 6);

  long long D[25];
  printf("=== CLRS Fig 25.1 Floyd-Warshall ===\n");
  floyd_warshall(W, 5, D);
  print_mat("delta", D, 5);

  printf("\n=== Johnson (same graph) ===\n");
  johnson(W, 5, D);
  print_mat("delta", D, 5);
  return 0;
}
