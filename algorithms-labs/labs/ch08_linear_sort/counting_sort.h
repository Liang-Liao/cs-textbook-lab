#ifndef CLRS_COUNTING_SORT_H
#define CLRS_COUNTING_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 8.2 Counting sort.
 * Sorts a[0..n) where each element is an integer in [0, k].
 * Stable. Output written to b[0..n); a is unchanged.
 * k is the maximum allowed value (inclusive).
 */
void counting_sort_int(const int *a, int *b, size_t n, int k);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_COUNTING_SORT_H */
