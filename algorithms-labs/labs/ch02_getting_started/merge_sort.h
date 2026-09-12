#ifndef CLRS_MERGE_SORT_H
#define CLRS_MERGE_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 2.3 Merge Sort
 * Θ(n log n) all cases, not in-place (O(n) aux), stable.
 */
void merge_sort_int(int *a, size_t n);

/* Merge sorted A[p..q] and A[q+1..r] using aux buffer. */
void merge_int(int *a, size_t p, size_t q, size_t r, int *aux);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MERGE_SORT_H */
