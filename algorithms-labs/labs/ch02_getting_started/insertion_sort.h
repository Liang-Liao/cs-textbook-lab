#ifndef CLRS_INSERTION_SORT_H
#define CLRS_INSERTION_SORT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 2.1 Insertion Sort
 * Best: Θ(n), Average/Worst: Θ(n²), in-place, stable.
 */
void insertion_sort_int(int *a, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_INSERTION_SORT_H */
