#ifndef CLRS_POLY_FFT_H
#define CLRS_POLY_FFT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.30 Polynomials and the FFT.
 * Polynomials of degree < n stored as coefficient arrays a[0..n-1]
 * (a[i] is coefficient of x^i). n must be a power of 2 for FFT path.
 */

/* Coefficient form multiply: C = A * B, degree(A)<n, degree(B)<n.
 * out length 2n. Naive O(n^2). */
void poly_multiply_naive(const double *A, const double *B, size_t n,
                         double *out /* 2n */);

/*
 * CLRS 30.5/30.6 FFT-based multiply.
 * n must be a power of 2. Pads A,B into length 2n with zeros.
 * Result coefficients written to out[0..2n-1].
 */
void poly_multiply_fft(const double *A, const double *B, size_t n,
                       double *out /* 2n */);

/* In-place iterative Cooley-Tukey FFT (or inverse if inverse=1).
 * re/im are separate arrays of length n (power of 2). */
void fft(double *re, double *im, size_t n, int inverse);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_POLY_FFT_H */
