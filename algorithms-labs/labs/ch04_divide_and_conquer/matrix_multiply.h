#ifndef CLRS_MATRIX_MULTIPLY_H
#define CLRS_MATRIX_MULTIPLY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dense matrices stored row-major as long long * with leading dimension n
 * (square n x n). Caller owns memory.
 */

/* CLRS 4.2 naive Θ(n³): C = A * B */
void matrix_multiply_naive(long long *C, const long long *A,
                           const long long *B, size_t n);

/* Recursive divide-and-conquer (CLRS 4.2), Θ(n³) still — educational. */
void matrix_multiply_recursive(long long *C, const long long *A,
                               const long long *B, size_t n);

/*
 * Strassen (CLRS 4.2). n must be a power of 2 (caller pads if needed).
 * Θ(n^{lg 7}) ≈ Θ(n^2.807).
 */
void matrix_multiply_strassen(long long *C, const long long *A,
                              const long long *B, size_t n);

/* C = A + B (square n) */
void matrix_add(long long *C, const long long *A, const long long *B, size_t n);

/* C = A - B (square n) */
void matrix_sub(long long *C, const long long *A, const long long *B, size_t n);

/* Zero an n x n matrix */
void matrix_zero(long long *C, size_t n);

/* Copy n x n matrix */
void matrix_copy(long long *C, const long long *A, size_t n);

/* 1 if equal */
int matrix_equal(const long long *A, const long long *B, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MATRIX_MULTIPLY_H */
