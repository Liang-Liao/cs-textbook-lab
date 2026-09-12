#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "matrix_ops.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Solve A x = b, A = [[2,0,2],[0.6,1.25,3],[0.4,0.2,1]] book-like
     simpler: A=[[2,1],[1,3]], b=[3,5] => x=(4/5, 7/5)? 2x+y=3, x+3y=5
     x=4/5, y=7/5 */
  {
    double A[] = {2, 1, 1, 3};
    double b[] = {3, 5};
    double x[2];
    double LU[4];
    int perm[2];
    for (int i = 0; i < 4; i++) {
      LU[i] = A[i];
    }
    ASSERT_TRUE(&t, lup_decompose(LU, 2, perm));
    ASSERT_TRUE(&t, lup_solve(LU, 2, perm, b, x));
    ASSERT_TRUE(&t, fabs(x[0] - 0.8) < 1e-9);
    ASSERT_TRUE(&t, fabs(x[1] - 1.4) < 1e-9);
  }

  /* identity */
  {
    double A[9];
    mat_identity(A, 3);
    double det = matrix_determinant(A, 3);
    ASSERT_TRUE(&t, fabs(det - 1.0) < 1e-9);
  }

  /* det[[1,2],[3,4]] = -2 */
  {
    double A[] = {1, 2, 3, 4};
    ASSERT_TRUE(&t, fabs(matrix_determinant(A, 2) + 2.0) < 1e-9);
  }

  /* det of triangular = product of diagonal */
  {
    double A[] = {2, 5, 0, 3};
    ASSERT_TRUE(&t, fabs(matrix_determinant(A, 2) - 6.0) < 1e-9);
  }

  /* singular */
  {
    double A[] = {1, 2, 2, 4};
    ASSERT_TRUE(&t, fabs(matrix_determinant(A, 2)) < 1e-9);
    double inv[4];
    ASSERT_TRUE(&t, !matrix_inverse(A, 2, inv));
  }

  /* inverse: [[1,2],[3,4]] inverse = [[-2,1],[1.5,-0.5]] */
  {
    double A[] = {1, 2, 3, 4};
    double inv[4];
    ASSERT_TRUE(&t, matrix_inverse(A, 2, inv));
    ASSERT_TRUE(&t, fabs(inv[0] + 2) < 1e-9);
    ASSERT_TRUE(&t, fabs(inv[1] - 1) < 1e-9);
    ASSERT_TRUE(&t, fabs(inv[2] - 1.5) < 1e-9);
    ASSERT_TRUE(&t, fabs(inv[3] + 0.5) < 1e-9);

    /* A * Ainv ≈ I */
    double P[4];
    matmul_nn(A, inv, 2, P);
    double I[4];
    mat_identity(I, 2);
    ASSERT_TRUE(&t, mat_almost_equal(P, I, 2, 1e-9));
  }

  /* 3x3 inverse roundtrip */
  {
    double A[] = {4, 2, 1, 4, 5, 3, 2, 3, 1};
    double inv[9], P[9], I[9];
    ASSERT_TRUE(&t, matrix_inverse(A, 3, inv));
    matmul_nn(A, inv, 3, P);
    mat_identity(I, 3);
    ASSERT_TRUE(&t, mat_almost_equal(P, I, 3, 1e-8));
  }

  /* permute rows needed for pivot: [[0,1],[1,0]] swap */
  {
    double A[] = {0, 1, 1, 0};
    double LU[4];
    int perm[2];
    for (int i = 0; i < 4; i++) {
      LU[i] = A[i];
    }
    ASSERT_TRUE(&t, lup_decompose(LU, 2, perm));
    double b[] = {1, 0};
    double x[2];
    ASSERT_TRUE(&t, lup_solve(LU, 2, perm, b, x));
    /* Ax=b => x=(0,1) */
    ASSERT_TRUE(&t, fabs(x[0] - 0.0) < 1e-9);
    ASSERT_TRUE(&t, fabs(x[1] - 1.0) < 1e-9);
  }

  /* tiny-scale well-conditioned matrix: must NOT be judged singular.
     A = 1e-20 * [[1,2],[3,7]], det = 1e-40, x for b=(4,10)*1e-20 is
     (8,-2) exactly. */
  {
    double A[4] = {1e-20, 2e-20, 3e-20, 7e-20};
    double det = matrix_determinant(A, 2);
    ASSERT_TRUE(&t, det != 0.0);
    double LU[4];
    int perm[2];
    double b[2] = {4e-20, 10e-20};
    double x[2];
    for (int i = 0; i < 4; i++) {
      LU[i] = A[i];
    }
    ASSERT_EQ_INT(&t, lup_decompose(LU, 2, perm), 1);
    ASSERT_EQ_INT(&t, lup_solve(LU, 2, perm, b, x), 1);
    ASSERT_TRUE(&t, fabs(x[0] - 8.0) < 1e-6);
    ASSERT_TRUE(&t, fabs(x[1] + 2.0) < 1e-6);
  }

  return test_report(&t, "matrix_ops");
}
