#include "counting_sort.h"

#include <stdlib.h>

#include "clrs.h"

/*
 * CLRS COUNTING-SORT(A, B, k), 0-based:
 *   for i = 0 .. k: C[i] = 0
 *   for j = 0 .. n-1: C[A[j]]++
 *   for i = 1 .. k: C[i] += C[i-1]
 *   for j = n-1 downto 0:
 *     B[C[A[j]]-1] = A[j]
 *     C[A[j]]--
 */
void counting_sort_int(const int *a, int *b, size_t n, int k) {
  CLRS_ASSERT(k >= 0, "k must be >= 0");
  if (n == 0) {
    return;
  }

  size_t csize = (size_t)k + 1;
  size_t *C = clrs_xcalloc(csize, sizeof(size_t));

  for (size_t j = 0; j < n; j++) {
    CLRS_ASSERT(a[j] >= 0 && a[j] <= k, "element out of range [0,k]");
    C[a[j]]++;
  }

  for (int i = 1; i <= k; i++) {
    C[i] += C[i - 1];
  }

  for (size_t j = n; j-- > 0;) {
    int x = a[j];
    C[x]--;
    b[C[x]] = x;
  }

  free(C);
}
