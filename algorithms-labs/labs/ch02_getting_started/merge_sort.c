#include "merge_sort.h"

#include <stdlib.h>

#include "clrs.h"

/*
 * Merge (CLRS 2.3), 1-based pseudocode adapted to 0-based indices.
 * A[p..q] and A[q+1..r] are sorted; aux must hold at least r-p+1 ints.
 */
void merge_int(int *a, size_t p, size_t q, size_t r, int *aux) {
  size_t n1 = q - p + 1;
  size_t n2 = r - q;

  for (size_t i = 0; i < n1; i++) {
    aux[i] = a[p + i];
  }
  for (size_t j = 0; j < n2; j++) {
    aux[n1 + j] = a[q + 1 + j];
  }

  size_t i = 0;
  size_t j = 0;
  size_t k = p;
  while (i < n1 && j < n2) {
    if (aux[i] <= aux[n1 + j]) {
      a[k++] = aux[i++];
    } else {
      a[k++] = aux[n1 + j++];
    }
  }
  while (i < n1) {
    a[k++] = aux[i++];
  }
  while (j < n2) {
    a[k++] = aux[n1 + j++];
  }
}

static void merge_sort_rec(int *a, size_t p, size_t r, int *aux) {
  if (p >= r) {
    return;
  }
  size_t q = p + (r - p) / 2;
  merge_sort_rec(a, p, q, aux);
  merge_sort_rec(a, q + 1, r, aux);
  merge_int(a, p, q, r, aux);
}

void merge_sort_int(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  int *aux = clrs_xmalloc(n * sizeof(int));
  merge_sort_rec(a, 0, n - 1, aux);
  free(aux);
}
