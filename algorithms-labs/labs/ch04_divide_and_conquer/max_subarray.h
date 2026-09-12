#ifndef CLRS_MAX_SUBARRAY_H
#define CLRS_MAX_SUBARRAY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  size_t low;    /* inclusive */
  size_t high;   /* inclusive */
  long long sum; /* a[low] + ... + a[high] */
} MaxSubarray;

/*
 * CLRS 4.1 Maximum-subarray, divide-and-conquer.
 * O(n log n). Array must be non-empty (n >= 1).
 */
MaxSubarray max_subarray_dc(const long long *a, size_t n);

/*
 * Brute force O(n^2), for testing and comparison.
 */
MaxSubarray max_subarray_brute(const long long *a, size_t n);

/*
 * Kadane O(n) — CLRS exercise-style linear scan (optional companion).
 */
MaxSubarray max_subarray_kadane(const long long *a, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MAX_SUBARRAY_H */
