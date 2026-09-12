#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "matrix_multiply.h"
#include "test.h"

static void fill_mat(long long *M, size_t n, uint32_t seed) {
  int *tmp = clrs_xmalloc(n * n * sizeof(int));
  array_fill_random(tmp, n * n, seed, -5, 5);
  for (size_t i = 0; i < n * n; i++) {
    M[i] = tmp[i];
  }
  free(tmp);
}

static void identity(long long *M, size_t n) {
  matrix_zero(M, n);
  for (size_t i = 0; i < n; i++) {
    M[i * n + i] = 1;
  }
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* 1x1 */
  {
    long long a[1] = {3}, b[1] = {4}, c[1] = {0};
    matrix_multiply_naive(c, a, b, 1);
    ASSERT_EQ_INT(&t, c[0], 12);
    matrix_multiply_recursive(c, a, b, 1);
    ASSERT_EQ_INT(&t, c[0], 12);
    matrix_multiply_strassen(c, a, b, 1);
    ASSERT_EQ_INT(&t, c[0], 12);
  }

  /* Book-style 2x2
     * A = [[1,3],[7,5]]  B = [[6,8],[4,2]]
     * C = [[18,14],[62,66]] */
  {
    long long A[] = {1, 3, 7, 5};
    long long B[] = {6, 8, 4, 2};
    long long C[4];
    matrix_multiply_naive(C, A, B, 2);
    ASSERT_EQ_INT(&t, C[0], 18);
    ASSERT_EQ_INT(&t, C[1], 14);
    ASSERT_EQ_INT(&t, C[2], 62);
    ASSERT_EQ_INT(&t, C[3], 66);

    long long Cr[4], Cs[4];
    matrix_multiply_recursive(Cr, A, B, 2);
    matrix_multiply_strassen(Cs, A, B, 2);
    ASSERT_TRUE(&t, matrix_equal(C, Cr, 2));
    ASSERT_TRUE(&t, matrix_equal(C, Cs, 2));
  }

  /* Identity */
  {
    size_t n = 8;
    long long *A = clrs_xmalloc(n * n * sizeof(long long));
    long long *I = clrs_xmalloc(n * n * sizeof(long long));
    long long *C = clrs_xmalloc(n * n * sizeof(long long));
    fill_mat(A, n, 1u);
    identity(I, n);

    matrix_multiply_naive(C, A, I, n);
    ASSERT_TRUE(&t, matrix_equal(C, A, n));
    matrix_multiply_strassen(C, A, I, n);
    ASSERT_TRUE(&t, matrix_equal(C, A, n));
    matrix_multiply_recursive(C, A, I, n);
    ASSERT_TRUE(&t, matrix_equal(C, A, n));

    free(A);
    free(I);
    free(C);
  }

  /* Cross-check naive vs recursive vs Strassen on powers of 2 */
  {
    const size_t sizes[] = {2, 4, 8, 16, 32, 64};
    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
      size_t n = sizes[si];
      size_t bytes = n * n * sizeof(long long);
      long long *A = clrs_xmalloc(bytes);
      long long *B = clrs_xmalloc(bytes);
      long long *C1 = clrs_xmalloc(bytes);
      long long *C2 = clrs_xmalloc(bytes);
      long long *C3 = clrs_xmalloc(bytes);

      fill_mat(A, n, 100u + (uint32_t)n);
      fill_mat(B, n, 200u + (uint32_t)n);

      matrix_multiply_naive(C1, A, B, n);
      matrix_multiply_recursive(C2, A, B, n);
      matrix_multiply_strassen(C3, A, B, n);

      if (!matrix_equal(C1, C2, n)) {
        fprintf(stderr, "  recursive mismatch at n=%zu\n", n);
      }
      if (!matrix_equal(C1, C3, n)) {
        fprintf(stderr, "  strassen mismatch at n=%zu\n", n);
      }
      ASSERT_TRUE(&t, matrix_equal(C1, C2, n));
      ASSERT_TRUE(&t, matrix_equal(C1, C3, n));

      free(A);
      free(B);
      free(C1);
      free(C2);
      free(C3);
    }
  }

  /* Non-power-of-2 recursive falls back / still correct */
  {
    size_t n = 5;
    long long A[25], B[25], C1[25], C2[25];
    fill_mat(A, n, 3u);
    fill_mat(B, n, 4u);
    matrix_multiply_naive(C1, A, B, n);
    matrix_multiply_recursive(C2, A, B, n);
    ASSERT_TRUE(&t, matrix_equal(C1, C2, n));
  }

  return test_report(&t, "matrix_multiply");
}
