#include "insertion_sort.h"

/*
 * Pseudocode (CLRS 2.1), arrays are 1-based there:
 *
 *   for j = 2 to A.length
 *     key = A[j]
 *     i = j - 1
 *     while i > 0 and A[i] > key
 *       A[i+1] = A[i]
 *       i = i - 1
 *     A[i+1] = key
 *
 * C adaptation: index from 0, outer loop starts at j = 1.
 */
void insertion_sort_int(int *a, size_t n) {
  for (size_t j = 1; j < n; j++) {
    int key = a[j];
    size_t i = j;
    while (i > 0 && a[i - 1] > key) {
      a[i] = a[i - 1];
      i--;
    }
    a[i] = key;
  }
}
