/* Demo: greedy algorithms (CLRS Ch.15). */
#include <stdio.h>

#include "greedy.h"

int main(void) {
  printf("=== CLRS 16.1 Activity selection ===\n");
  Activity acts[] = {
      {1, 4}, {3, 5}, {0, 6}, {5, 7}, {3, 9}, {5, 9}, {6, 10}, {8, 11},
      {8, 12}, {2, 14}, {12, 16}};
  size_t sel[16];
  size_t n = activity_select(acts, 11, sel);
  printf("selected %zu activities:", n);
  for (size_t i = 0; i < n; i++) {
    printf(" [%d,%d)", acts[sel[i]].start, acts[sel[i]].finish);
  }
  printf("\n");

  printf("\n=== Fractional knapsack ===\n");
  double w[] = {10, 20, 30};
  double v[] = {60, 100, 120};
  double take[3];
  double val = fractional_knapsack(w, v, 3, 50.0, take);
  printf("capacity 50: value=%.1f  take=[%.2f, %.2f, %.2f]\n", val, take[0],
         take[1], take[2]);

  printf("\n=== CLRS 16.3 Huffman ===\n");
  int freq[] = {45, 13, 12, 16, 9, 5}; /* a..f */
  int depth[6];
  double wpl = huffman_wpl(freq, 6, depth);
  printf("frequencies 45,13,12,16,9,5\n");
  printf("code lengths: a=%d b=%d c=%d d=%d e=%d f=%d\n", depth[0], depth[1],
         depth[2], depth[3], depth[4], depth[5]);
  printf("weighted path length = %.0f (book: 224)\n", wpl);

  return 0;
}
