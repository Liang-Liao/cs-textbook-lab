#ifndef CLRS_QUICKSORT_H
#define CLRS_QUICKSORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.7 Quicksort — 0-based array adaptation of book's 1-based pseudocode.
 */

/* CLRS 7.1 PARTITION (Lomuto). Returns pivot final index. */
size_t partition_int(int *a, size_t p, size_t r);

/* CLRS 7.2 QUICKSORT */
void quicksort_int(int *a, size_t n);

/* CLRS 7.3 RANDOMIZED-QUICKSORT */
void randomized_quicksort_int(int *a, size_t n);

/* Randomized partition using clrs_rand_int (caller seeds RNG). */
size_t randomized_partition_int(int *a, size_t p, size_t r);

/* Hoare partition (CLRS problem 7-1). Returns pivot split index j. */
size_t hoare_partition_int(int *a, size_t p, size_t r);

void hoare_quicksort_int(int *a, size_t n);

/* Seed helper so labs/tests can use a fixed seed without linking ch05. */
void quicksort_srand(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_QUICKSORT_H */
