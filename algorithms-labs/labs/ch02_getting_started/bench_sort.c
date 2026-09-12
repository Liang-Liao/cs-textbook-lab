/* Bench: insertion vs merge sort on growing n (CLRS Ch.2). */
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "insertion_sort.h"
#include "merge_sort.h"
#include "timing.h"

static void bench_one(const char *name, size_t n, void (*sort)(int *, size_t),
                      uint32_t seed) {
  int *a = clrs_xmalloc(n * sizeof(int));
  array_fill_random(a, n, seed, -1000000, 1000000);
  int64_t t0 = clrs_now_us();
  sort(a, n);
  int64_t dt = clrs_elapsed_us(t0);
  if (!array_is_sorted_int(a, n)) {
    fprintf(stderr, "  %s n=%zu NOT SORTED\n", name, n);
  }
  printf("  %-12s n=%6zu  %8lld us\n", name, n, (long long)dt);
  free(a);
}

int main(void) {
  const size_t sizes[] = {1000, 4000, 8000};
  printf("=== Sort timing (random ints, CLRS Ch.2) ===\n");
  for (size_t si = 0; si < 3; si++) {
    size_t n = sizes[si];
    bench_one("insertion", n, insertion_sort_int, 1u + (uint32_t)n);
    bench_one("merge", n, merge_sort_int, 2u + (uint32_t)n);
    printf("\n");
  }
  return 0;
}
