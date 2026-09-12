#ifndef CLRS_RADIX_SORT_H
#define CLRS_RADIX_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 8.3 Radix sort.
 * Sorts non-negative integers by digit (base 10 by default).
 * Stable digit sort uses counting sort on each digit.
 */
void radix_sort_int(int *a, size_t n);

/* Generalized: digits in given base (base >= 2). */
void radix_sort_int_base(int *a, size_t n, int base);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_RADIX_SORT_H */
