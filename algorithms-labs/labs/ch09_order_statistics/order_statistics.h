#ifndef CLRS_ORDER_STATISTICS_H
#define CLRS_ORDER_STATISTICS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.9 Medians and order statistics.
 * All select functions work on a mutable int array of size n.
 * Order i is 0-based: i=0 is minimum, i=n-1 is maximum.
 * (Book uses 1-based i; subtract 1 when comparing to the text.)
 */

typedef struct {
  int min;
  int max;
} MinMax;

/* CLRS 9.1: scan for min and max separately. */
MinMax min_max_pair(const int *a, size_t n);

/* CLRS 9.1 exercise-style: find both in ~3n/2 comparisons. */
MinMax min_max_optimized(const int *a, size_t n);

/* CLRS 9.2 RANDOMIZED-SELECT. 0 <= i < n. May reorder the array. */
int randomized_select_int(int *a, size_t n, size_t i);

/* CLRS 9.3 SELECT (median-of-medians). 0 <= i < n. Worst-case O(n). */
int select_int(int *a, size_t n, size_t i);

void order_stat_srand(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_ORDER_STATISTICS_H */
