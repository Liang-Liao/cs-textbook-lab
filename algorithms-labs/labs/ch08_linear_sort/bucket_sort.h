#ifndef CLRS_BUCKET_SORT_H
#define CLRS_BUCKET_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 8.4 Bucket sort for floats in [0, 1).
 * Input values should satisfy 0 <= a[i] < 1.
 * n buckets; insert into bucket floor(n*a[i]); sort each with insertion sort.
 */
void bucket_sort_double(double *a, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_BUCKET_SORT_H */
