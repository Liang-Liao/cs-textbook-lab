#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "greedy.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book activity set (CLRS 16.1) */
  {
    Activity acts[] = {
        {1, 4}, {3, 5}, {0, 6}, {5, 7}, {3, 9}, {5, 9}, {6, 10}, {8, 11},
        {8, 12}, {2, 14}, {12, 16}};
    size_t sel[16];
    size_t n = activity_select(acts, 11, sel);
    ASSERT_EQ_INT(&t, (int)n, 4);
    /* After sort by finish, greedy picks a1(1,4), a4(5,7), a8(8,11), a11(12,16) */
    ASSERT_EQ_INT(&t, acts[sel[0]].start, 1);
    ASSERT_EQ_INT(&t, acts[sel[0]].finish, 4);
    ASSERT_EQ_INT(&t, acts[sel[1]].start, 5);
    ASSERT_EQ_INT(&t, acts[sel[1]].finish, 7);
    ASSERT_EQ_INT(&t, acts[sel[2]].start, 8);
    ASSERT_EQ_INT(&t, acts[sel[2]].finish, 11);
    ASSERT_EQ_INT(&t, acts[sel[3]].start, 12);
    ASSERT_EQ_INT(&t, acts[sel[3]].finish, 16);
  }

  {
    Activity one[] = {{0, 5}};
    size_t sel[4];
    ASSERT_EQ_INT(&t, (int)activity_select(one, 1, sel), 1);
  }

  {
    Activity none_compat[] = {{0, 10}, {1, 11}, {2, 12}};
    size_t sel[4];
    ASSERT_EQ_INT(&t, (int)activity_select(none_compat, 3, sel), 1);
  }

  /* fractional knapsack: items w=10,v=60; w=20,v=100; w=30,v=120; cap=50
   * ratios 6, 5, 4 → take all of first two (30), 20/30 of third
   * value = 60+100+80 = 240 */
  {
    double w[] = {10, 20, 30};
    double v[] = {60, 100, 120};
    double take[3];
    double val = fractional_knapsack(w, v, 3, 50.0, take);
    ASSERT_TRUE(&t, fabs(val - 240.0) < 1e-9);
    ASSERT_TRUE(&t, fabs(take[0] - 1.0) < 1e-9);
    ASSERT_TRUE(&t, fabs(take[1] - 1.0) < 1e-9);
    ASSERT_TRUE(&t, fabs(take[2] - 2.0 / 3.0) < 1e-9);
  }

  {
    double w[] = {1};
    double v[] = {10};
    double take[1];
    double val = fractional_knapsack(w, v, 1, 0.5, take);
    ASSERT_TRUE(&t, fabs(val - 5.0) < 1e-9);
    ASSERT_TRUE(&t, fabs(take[0] - 0.5) < 1e-9);
  }

  /* Huffman: book example a:45 b:13 c:12 d:16 e:9 f:5 → WPL=224
   * (CLRS 16.3 book example) */
  {
    int freq[] = {45, 13, 12, 16, 9, 5};
    int depth[6];
    double wpl = huffman_wpl(freq, 6, depth);
    ASSERT_TRUE(&t, fabs(wpl - 224.0) < 1e-9);
    ASSERT_EQ_INT(&t, depth[0], 1); /* a has depth 1 */
    ASSERT_TRUE(&t, depth[5] >= 3); /* rarest has longer code */
  }

  {
    int freq[] = {1};
    int depth[1];
    ASSERT_TRUE(&t, fabs(huffman_wpl(freq, 1, depth) - 0.0) < 1e-9);
  }

  {
    int freq[] = {1, 1};
    int depth[2];
    ASSERT_TRUE(&t, fabs(huffman_wpl(freq, 2, depth) - 2.0) < 1e-9);
  }

  return test_report(&t, "greedy");
}
