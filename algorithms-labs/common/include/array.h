#ifndef CLRS_ARRAY_H
#define CLRS_ARRAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fill patterns for tests and demos. */
void array_fill_random(int *a, size_t n, uint32_t seed, int min_val,
                       int max_val);
void array_fill_range(int *a, size_t n, int start);
void array_fill_ascending(int *a, size_t n);
void array_fill_descending(int *a, size_t n);
void array_fill_constant(int *a, size_t n, int value);

void array_copy_int(int *dst, const int *src, size_t n);

/* Returns 1 if non-decreasing. */
int array_is_sorted_int(const int *a, size_t n);

/* Returns 1 if same length multiset (order-insensitive). */
int array_same_multiset_int(const int *a, const int *b, size_t n);

void array_print_int(const char *label, const int *a, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_ARRAY_H */
