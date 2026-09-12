/* Demo: dynamic table amortized growth (CLRS Ch.16/17). */
#include <stdio.h>

#include "dynamic_table.h"

int main(void) {
  DynTable dt;
  dtab_init(&dt);

  printf("=== CLRS Dynamic table: expansion ===\n");
  printf("%6s %6s %12s\n", "n", "size", "total_moves");
  for (int i = 1; i <= 16; i++) {
    dtab_push(&dt, i);
    printf("%6zu %6zu %12lu\n", dt.n, dt.size, dt.resizes);
  }

  printf("\n=== Contraction ===\n");
  while (dt.n > 0) {
    unsigned long before = dt.resizes;
    int x = dtab_pop(&dt);
    (void)x;
    if (dt.resizes != before) {
      printf("after pop: n=%zu size=%zu total_moves=%lu\n", dt.n, dt.size,
             dt.resizes);
    }
  }

  dtab_destroy(&dt);

  printf("\n=== n inserts: total element moves = n-1 ===\n");
  dtab_init(&dt);
  const int N = 1000;
  for (int i = 0; i < N; i++) {
    dtab_push(&dt, i);
  }
  printf("n=%d  size=%zu  resizes(moves)=%lu  amortized moves/op=%.3f\n", N,
         dt.size, dt.resizes, (double)dt.resizes / (double)N);
  dtab_destroy(&dt);
  return 0;
}
