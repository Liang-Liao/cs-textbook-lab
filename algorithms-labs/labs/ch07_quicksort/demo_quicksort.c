/* Demo: quicksort variants (CLRS Ch.7). */
#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "quicksort.h"

static void show(const char *label, int *a, size_t n, void (*sort)(int *, size_t)) {
  int *b = clrs_xmalloc(n * sizeof(int));
  array_copy_int(b, a, n);
  sort(b, n);
  array_print_int(label, b, n);
  free(b);
}

int main(void) {
  int a[] = {2, 8, 7, 1, 3, 5, 6, 4};
  size_t n = sizeof(a) / sizeof(a[0]);

  printf("=== CLRS 7.1 Partition ===\n");
  array_print_int("input", a, n);
  int *p = clrs_xmalloc(n * sizeof(int));
  array_copy_int(p, a, n);
  size_t q = partition_int(p, 0, n - 1);
  array_print_int("after", p, n);
  printf("pivot index q = %zu, A[q] = %d\n", q, p[q]);
  free(p);

  printf("\n=== CLRS 7.2 Quicksort variants ===\n");
  array_print_int("input", a, n);
  show("quicksort", a, n, quicksort_int);
  quicksort_srand(7u);
  show("randomized", a, n, randomized_quicksort_int);
  show("hoare", a, n, hoare_quicksort_int);

  return 0;
}
