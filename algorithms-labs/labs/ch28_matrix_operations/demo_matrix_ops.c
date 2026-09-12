/* Demo: LUP, solve, det, inverse (CLRS Ch.28). */
#include <stdio.h>

#include "matrix_ops.h"

int main(void) {
  double A[] = {1, 2, 3, 4};
  double inv[4];
  printf("=== CLRS 28 LUP / inverse ===\n");
  printf("A = [[1,2],[3,4]]\n");
  printf("det = %.6f (expect -2)\n", matrix_determinant(A, 2));
  if (matrix_inverse(A, 2, inv)) {
    printf("A^{-1} = [[%.4f, %.4f], [%.4f, %.4f]]\n", inv[0], inv[1], inv[2],
           inv[3]);
  }

  double LU[4];
  int perm[2];
  for (int i = 0; i < 4; i++) {
    LU[i] = A[i];
  }
  lup_decompose(LU, 2, perm);
  double b[] = {5, 11};
  double x[2];
  lup_solve(LU, 2, perm, b, x);
  printf("Solve A x = [5,11]: x = [%.4f, %.4f]\n", x[0], x[1]);
  return 0;
}
