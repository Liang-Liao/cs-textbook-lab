/* Demo: min/max and selection (CLRS Ch.9). */
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "order_statistics.h"

int main(void) {
  order_stat_srand(1u);

  int a[] = {12, 3, 5, 7, 19, 1, 8, 4};
  const size_t n = sizeof(a) / sizeof(a[0]);

  printf("=== CLRS 9.1 Min and max ===\n");
  array_print_int("A", a, n);
  MinMax r = min_max_optimized(a, n);
  printf("min=%d max=%d\n", r.min, r.max);

  printf("\n=== CLRS 9.2 / 9.3 Order statistics ===\n");
  printf("sorted ranks 0..%zu should be: ", n - 1);
  int *s = clrs_xmalloc(n * sizeof(int));
  array_copy_int(s, a, n);
  qsort(s, n, sizeof(int), clrs_cmp_int);
  for (size_t i = 0; i < n; i++) {
    printf("%d ", s[i]);
  }
  printf("\n");

  int *w = clrs_xmalloc(n * sizeof(int));
  array_copy_int(w, a, n);
  printf("randomized_select(3)=%d  (median-ish)\n", randomized_select_int(w, n, 3));
  array_copy_int(w, a, n);
  printf("select(0)=%d select(7)=%d\n", select_int(w, n, 0),
         select_int(w, n, 7));

  free(s);
  free(w);
  return 0;
}
