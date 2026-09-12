#include <stdio.h>
#include <stdlib.h>

#include "all_pairs.h"
#include "clrs.h"
#include "test.h"

/* CLRS Fig 25.1: 5-vertex graph (s,t,x,y,z) */
static void build_fig251(long long *W, size_t n) {
  apsp_init_matrix(W, n);
  apsp_set_edge(W, n, 0, 1, 3);
  apsp_set_edge(W, n, 0, 4, -4);
  apsp_set_edge(W, n, 0, 2, 8);
  apsp_set_edge(W, n, 1, 3, 1);
  apsp_set_edge(W, n, 1, 4, 7);
  apsp_set_edge(W, n, 2, 1, 4);
  apsp_set_edge(W, n, 3, 0, 2);
  apsp_set_edge(W, n, 3, 2, -5);
  apsp_set_edge(W, n, 4, 3, 6);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Floyd-Warshall Fig 25.1 — book delta matrix:
   * 0  1 -3  2 -4
   * 3  0 -4  1 -1
   * 7  4  0  5  3
   * 2 -1 -5  0 -2
   * 8  5  1  6  0
   */
  {
    long long W[25], D[25];
    build_fig251(W, 5);
    ASSERT_TRUE(&t, floyd_warshall(W, 5, D));
    long long exp[25] = {
        0, 1, -3, 2, -4,
        3, 0, -4, 1, -1,
        7, 4, 0, 5, 3,
        2, -1, -5, 0, -2,
        8, 5, 1, 6, 0};
    for (int i = 0; i < 25; i++) {
      ASSERT_EQ_INT(&t, (int)D[i], (int)exp[i]);
    }
  }

  /* Johnson agrees with Floyd-Warshall on same graph */
  {
    long long W[25], Dfw[25], Djohn[25];
    build_fig251(W, 5);
    ASSERT_TRUE(&t, floyd_warshall(W, 5, Dfw));
    ASSERT_TRUE(&t, johnson(W, 5, Djohn));
    for (int i = 0; i < 25; i++) {
      if (Dfw[i] != Djohn[i]) {
        fprintf(stderr, "  mismatch at %d: fw=%lld johnson=%lld\n", i, Dfw[i],
                Djohn[i]);
      }
      ASSERT_EQ_INT(&t, (int)Dfw[i], (int)Djohn[i]);
    }
  }

  /* simple triangle, non-negative */
  {
    long long W[9], D[9];
    apsp_init_matrix(W, 3);
    apsp_set_edge(W, 3, 0, 1, 1);
    apsp_set_edge(W, 3, 1, 2, 2);
    apsp_set_edge(W, 3, 0, 2, 10);
    ASSERT_TRUE(&t, floyd_warshall(W, 3, D));
    ASSERT_EQ_INT(&t, (int)D[0 * 3 + 2], 3); /* 0-1-2 */
    ASSERT_TRUE(&t, D[2 * 3 + 0] == APSP_INF);

    long long Dj[9];
    ASSERT_TRUE(&t, johnson(W, 3, Dj));
    ASSERT_EQ_INT(&t, (int)Dj[0 * 3 + 2], 3);
  }

  /* negative cycle */
  {
    long long W[3], D[3];
    apsp_init_matrix(W, 3);
    apsp_set_edge(W, 3, 0, 1, 1);
    apsp_set_edge(W, 3, 1, 2, -1);
    apsp_set_edge(W, 3, 2, 1, -1); /* 1-2-1 = -2 */
    ASSERT_TRUE(&t, !floyd_warshall(W, 3, D));
    ASSERT_TRUE(&t, !johnson(W, 3, D));
  }

  /* single vertex */
  {
    long long W[1], D[1];
    apsp_init_matrix(W, 1);
    ASSERT_TRUE(&t, floyd_warshall(W, 1, D));
    ASSERT_EQ_INT(&t, (int)D[0], 0);
    ASSERT_TRUE(&t, johnson(W, 1, D));
    ASSERT_EQ_INT(&t, (int)D[0], 0);
  }

  /* disconnected */
  {
    long long W[4], D[4];
    apsp_init_matrix(W, 2);
    apsp_set_edge(W, 2, 0, 1, 5);
    ASSERT_TRUE(&t, floyd_warshall(W, 2, D));
    ASSERT_EQ_INT(&t, (int)D[0 * 2 + 1], 5);
    ASSERT_TRUE(&t, D[1 * 2 + 0] == APSP_INF);
  }

  return test_report(&t, "all_pairs");
}
