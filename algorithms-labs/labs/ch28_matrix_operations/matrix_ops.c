#include "matrix_ops.h"

#include <math.h>
#include <stdlib.h>

#include "clrs.h"

/* CLRS 28.3 LUP-DECOMPOSITION with partial pivoting.
 * A pivot is treated as zero when it falls below 1e-12 x (largest |A|),
 * so the singularity test is scale-invariant. */
int lup_decompose(double *A, size_t n, int *perm) {
  double scale = 0.0;
  for (size_t i = 0; i < n * n; i++) {
    double v = fabs(A[i]);
    if (v > scale) {
      scale = v;
    }
  }
  for (size_t i = 0; i < n; i++) {
    perm[i] = (int)i;
  }

  for (size_t k = 0; k < n; k++) {
    size_t pivot = k;
    double p = fabs(A[k * n + k]);
    for (size_t i = k + 1; i < n; i++) {
      double v = fabs(A[i * n + k]);
      if (v > p) {
        p = v;
        pivot = i;
      }
    }
    if (p <= 1e-12 * scale) {
      return 0; /* singular (pivot ~ 0 relative to matrix scale) */
    }
    if (pivot != k) {
      /* swap rows k and pivot */
      for (size_t j = 0; j < n; j++) {
        double tmp = A[k * n + j];
        A[k * n + j] = A[pivot * n + j];
        A[pivot * n + j] = tmp;
      }
      int tp = perm[k];
      perm[k] = perm[pivot];
      perm[pivot] = tp;
    }
    for (size_t i = k + 1; i < n; i++) {
      A[i * n + k] /= A[k * n + k];
      double lik = A[i * n + k];
      for (size_t j = k + 1; j < n; j++) {
        A[i * n + j] -= lik * A[k * n + j];
      }
    }
  }
  return 1;
}

int lup_solve(const double *LU, size_t n, const int *perm, const double *b,
              double *x) {
  double *y = clrs_xmalloc(n * sizeof(double));
  /* scale reference: U's upper triangle carries the matrix magnitude
   * (the L part holds dimensionless multipliers), keeping the pivot
   * threshold scale-invariant */
  double scale = 0.0;
  for (size_t i = 0; i < n; i++) {
    for (size_t j = i; j < n; j++) {
      double v = fabs(LU[i * n + j]);
      if (v > scale) {
        scale = v;
      }
    }
  }
  /* Forward: L y = Pb  (L unit lower) */
  for (size_t i = 0; i < n; i++) {
    double s = b[perm[i]];
    for (size_t j = 0; j < i; j++) {
      s -= LU[i * n + j] * y[j];
    }
    y[i] = s;
  }
  /* Back: U x = y */
  for (size_t i = n; i-- > 0;) {
    double s = y[i];
    for (size_t j = i + 1; j < n; j++) {
      s -= LU[i * n + j] * x[j];
    }
    if (fabs(LU[i * n + i]) <= 1e-12 * scale) {
      free(y);
      return 0;
    }
    x[i] = s / LU[i * n + i];
  }
  free(y);
  return 1;
}

double matrix_determinant(const double *A, size_t n) {
  if (n == 0) {
    return 1.0;
  }
  double *M = clrs_xmalloc(n * n * sizeof(double));
  int *perm = clrs_xmalloc(n * sizeof(int));
  for (size_t i = 0; i < n * n; i++) {
    M[i] = A[i];
  }
  if (!lup_decompose(M, n, perm)) {
    free(M);
    free(perm);
    return 0.0;
  }
  double det = 1.0;
  int sign = 1;
  for (size_t i = 0; i < n; i++) {
    det *= M[i * n + i];
  }
  /* count inversions in perm for sign */
  for (size_t i = 0; i < n; i++) {
    for (size_t j = i + 1; j < n; j++) {
      if (perm[i] > perm[j]) {
        sign = -sign;
      }
    }
  }
  free(M);
  free(perm);
  return (double)sign * det;
}

void matmul_nn(const double *A, const double *B, size_t n, double *C) {
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      double s = 0.0;
      for (size_t k = 0; k < n; k++) {
        s += A[i * n + k] * B[k * n + j];
      }
      C[i * n + j] = s;
    }
  }
}

void mat_identity(double *A, size_t n) {
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      A[i * n + j] = (i == j) ? 1.0 : 0.0;
    }
  }
}

int mat_almost_equal(const double *A, const double *B, size_t n, double eps) {
  for (size_t i = 0; i < n * n; i++) {
    if (fabs(A[i] - B[i]) > eps) {
      return 0;
    }
  }
  return 1;
}

int matrix_inverse(const double *A, size_t n, double *inv_out) {
  double *LU = clrs_xmalloc(n * n * sizeof(double));
  int *perm = clrs_xmalloc(n * sizeof(int));
  double *b = clrs_xcalloc(n, sizeof(double));
  double *col = clrs_xmalloc(n * sizeof(double));

  for (size_t i = 0; i < n * n; i++) {
    LU[i] = A[i];
  }
  if (!lup_decompose(LU, n, perm)) {
    free(LU);
    free(perm);
    free(b);
    free(col);
    return 0;
  }

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++) {
      b[i] = (i == j) ? 1.0 : 0.0;
    }
    if (!lup_solve(LU, n, perm, b, col)) {
      free(LU);
      free(perm);
      free(b);
      free(col);
      return 0;
    }
    for (size_t i = 0; i < n; i++) {
      inv_out[i * n + j] = col[i];
    }
  }

  free(LU);
  free(perm);
  free(b);
  free(col);
  return 1;
}
