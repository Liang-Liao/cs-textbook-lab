#ifndef CLRS_MATRIX_OPS_H
#define CLRS_MATRIX_OPS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.28 Matrix operations.
 * Dense n x n row-major double matrices.
 */

/*
 * CLRS 28.3 LUP-DECOMPOSITION.
 * A is n*n (row-major). Writes L (unit lower, implicit 1s), U, and
 * permutation perm[0..n-1] (PI in the book, 0-based).
 * L stored in A_L below diagonal, U in A_U on/above diagonal.
 * Returns 1 on success, 0 if singular (pivot <= 1e-12 x largest |entry|).
 */
int lup_decompose(double *A, size_t n, int *perm);

/*
 * Solve A x = b given LUP factors in A and perm from lup_decompose.
 * b length n; writes solution into x (may alias b).
 * Returns 1 on success.
 */
int lup_solve(const double *LU, size_t n, const int *perm, const double *b,
              double *x);

/*
 * Determinant via LUP. Copies A internally. Returns det.
 * For singular matrices returns 0 (same relative pivot criterion).
 */
double matrix_determinant(const double *A, size_t n);

/*
 * Matrix inverse. A is n*n input; writes A^{-1} into inv_out.
 * Returns 1 if invertible, 0 if singular (relative pivot criterion).
 */
int matrix_inverse(const double *A, size_t n, double *inv_out);

/* C = A * B for n x n. */
void matmul_nn(const double *A, const double *B, size_t n, double *C);

/* Identity matrix. */
void mat_identity(double *A, size_t n);

/* 1 if all |A-B| <= eps */
int mat_almost_equal(const double *A, const double *B, size_t n, double eps);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_MATRIX_OPS_H */
