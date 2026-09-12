#ifndef CLRS_RANDOM_H
#define CLRS_RANDOM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Seed the process RNG used by clrs_rand_* (tests use a fixed seed). */
void clrs_srand(uint32_t seed);

/* Uniform integer in [low, high] inclusive. */
int clrs_rand_int(int low, int high);

/*
 * CLRS 5.3 RANDOMIZE-IN-PLACE (Fisher-Yates).
 * Permutes a[0..n-1] uniformly at random (in place).
 */
void clrs_randomize_in_place(int *a, size_t n);

/* Same permutation algorithms but with an explicit size-t element size. */
void clrs_randomize_in_place_bytes(void *base, size_t n, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_RANDOM_H */
