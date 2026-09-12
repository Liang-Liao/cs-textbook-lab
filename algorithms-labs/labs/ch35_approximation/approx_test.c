#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "approx.h"
#include "clrs.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* CLRS Fig 35.1-ish: triangle 0-1, 1-2, 2-0. Cover size <= 2*opt=2 */
  {
    int eu[] = {0, 1, 2};
    int ev[] = {1, 2, 0};
    int cover[4];
    size_t c = vertex_cover_approx(3, eu, ev, 3, cover, 4);
    ASSERT_TRUE(&t, c <= 4);
    ASSERT_TRUE(&t, c >= 2); /* triangle needs >= 2 */
    /* all edges covered */
    unsigned char hit[3] = {0, 0, 0};
    for (size_t i = 0; i < c; i++) {
      for (int e = 0; e < 3; e++) {
        if (eu[e] == cover[i] || ev[e] == cover[i]) {
          hit[e] = 1;
        }
      }
    }
    ASSERT_TRUE(&t, hit[0] && hit[1] && hit[2]);
  }

  /* path 0-1-2-3: optimal cover {1,2} size 2; approx may take 0,1 then 2,3 size 4? */
  {
    int eu[] = {0, 1, 2};
    int ev[] = {1, 2, 3};
    int cover[8];
    size_t c = vertex_cover_approx(4, eu, ev, 3, cover, 8);
    /* edges covered */
    int e0 = 0, e1 = 0, e2 = 0;
    for (size_t i = 0; i < c; i++) {
      if (cover[i] == 0 || cover[i] == 1) {
        e0 = 1;
      }
      if (cover[i] == 1 || cover[i] == 2) {
        e1 = 1;
      }
      if (cover[i] == 2 || cover[i] == 3) {
        e2 = 1;
      }
    }
    ASSERT_TRUE(&t, e0 && e1 && e2);
    /* 2-approx: |C| <= 2|OPT| = 4 */
    ASSERT_TRUE(&t, c <= 4);
  }

  /* single edge */
  {
    int eu[] = {0};
    int ev[] = {1};
    int cover[4];
    size_t c = vertex_cover_approx(2, eu, ev, 1, cover, 4);
    ASSERT_EQ_INT(&t, (int)c, 2);
  }

  /* star: center 0 to leaves 1..4 — optimal cover {0} size 1; approx takes
     both endpoints of first edge: 0 and 1, then all edges incident done → size 2 */
  {
    int eu[] = {0, 0, 0, 0};
    int ev[] = {1, 2, 3, 4};
    int cover[8];
    size_t c = vertex_cover_approx(5, eu, ev, 4, cover, 8);
    ASSERT_TRUE(&t, c <= 2); /* 2*opt = 2 */
  }

  /* TSP metric: square with unit sides, diagonal via triangle ineq 2
   * 0-1-2-3 cycle cost 4 */
  {
    double d[16];
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        d[i * 4 + j] = (i == j) ? 0.0 : 4.0; /* placeholder */
      }
    }
    /* 0=(0,0),1=(1,0),2=(1,1),3=(0,1) euclidean */
    double pts[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        double dx = pts[i][0] - pts[j][0];
        double dy = pts[i][1] - pts[j][1];
        d[i * 4 + j] = sqrt(dx * dx + dy * dy);
      }
    }
    int tour[8];
    double cost = tsp_metric_approx(d, 4, tour);
    ASSERT_TRUE(&t, cost > 3.9 && cost < 4.1);
    /* tour is permutation of 0..3 */
    int used[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; i++) {
      ASSERT_TRUE(&t, tour[i] >= 0 && tour[i] < 4);
      used[tour[i]] = 1;
    }
    ASSERT_TRUE(&t, used[0] && used[1] && used[2] && used[3]);
  }

  /* single city */
  {
    double d[1] = {0};
    int tour[1];
    ASSERT_TRUE(&t, fabs(tsp_metric_approx(d, 1, tour) - 0.0) < 1e-12);
  }

  /* two cities */
  {
    double d[4] = {0, 5, 5, 0};
    int tour[2];
    double cost = tsp_metric_approx(d, 2, tour);
    ASSERT_TRUE(&t, fabs(cost - 10.0) < 1e-9); /* 0-1-0 */
  }

  return test_report(&t, "approximation");
}
