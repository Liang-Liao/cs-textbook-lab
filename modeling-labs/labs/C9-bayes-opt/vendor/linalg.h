#ifndef MLAB_LINALG_H
#define MLAB_LINALG_H

#include "mat.h"

/*
 * Partial-pivoted LU: P A = L U.
 * lu is n x n overwritten with L (unit lower) and U (strict upper).
 * piv[i] = row swapped with i (row i final source is tracked via piv).
 * Returns 0 on success, -1 on singular / bad size.
 */
int mlab_lu_factor(double *lu, int n, int *piv);
/* Solve A x = b using factored lu/piv. x and b may alias. */
int mlab_lu_solve(const double *lu, int n, const int *piv, double *x, const double *b);
/* Convenience: factor A copy and solve. A is n x n row-major, not modified. */
int mlab_lu_solve_dense(const double *A, int n, const double *b, double *x);

/*
 * Cholesky A = L L^T for SPD A (row-major n x n).
 * l_out is n x n, lower triangular. Returns 0 on success, -1 if not SPD.
 */
int mlab_cholesky(const double *A, int n, double *l_out);
/* Solve L L^T x = b given factor l. */
int mlab_cholesky_solve(const double *l, int n, const double *b, double *x);

/*
 * Estimate 2-norm condition number of A via LU + power iteration on
 * A^T A and A^{-T} A^{-1} (rough but sufficient for lab thresholds).
 * For SPD use mlab_spd_cond2. Returns cond2 estimate (>0) or <0 on error.
 */
double mlab_cond2_estimate(const double *A, int n);
double mlab_spd_cond2(const double *A, int n);

/* Generate random-ish orthogonal n x n via repeated Householder on random cols (deterministic seed). */
int mlab_random_orthogonal(double *Q, int n, unsigned seed);
/*
 * Build A = Q1 * diag(sigma) * Q2^T with given singular values sigma[i],
 * so cond2(A) ≈ max(sigma)/min(sigma). Q1/Q2 orthogonal.
 */
int mlab_mat_from_svd_spectrum(double *A, int n, const double *sigma, unsigned seed);

/* SPD matrix A = Q diag(evals) Q^T with orthogonal Q (same basis both sides). */
int mlab_spd_from_spectrum(double *A, int n, const double *evals, unsigned seed);

#endif /* MLAB_LINALG_H */
