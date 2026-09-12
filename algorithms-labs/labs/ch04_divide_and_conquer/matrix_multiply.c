#include "matrix_multiply.h"

#include <stdlib.h>

#include "clrs.h"

#define IDX(i, j, n) ((i) * (n) + (j))

void matrix_zero(long long *C, size_t n) {
  for (size_t i = 0; i < n * n; i++) {
    C[i] = 0;
  }
}

void matrix_copy(long long *C, const long long *A, size_t n) {
  for (size_t i = 0; i < n * n; i++) {
    C[i] = A[i];
  }
}

void matrix_add(long long *C, const long long *A, const long long *B,
                size_t n) {
  for (size_t i = 0; i < n * n; i++) {
    C[i] = A[i] + B[i];
  }
}

void matrix_sub(long long *C, const long long *A, const long long *B,
                size_t n) {
  for (size_t i = 0; i < n * n; i++) {
    C[i] = A[i] - B[i];
  }
}

int matrix_equal(const long long *A, const long long *B, size_t n) {
  for (size_t i = 0; i < n * n; i++) {
    if (A[i] != B[i]) {
      return 0;
    }
  }
  return 1;
}

void matrix_multiply_naive(long long *C, const long long *A,
                           const long long *B, size_t n) {
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      long long s = 0;
      for (size_t k = 0; k < n; k++) {
        s += A[IDX(i, k, n)] * B[IDX(k, j, n)];
      }
      C[IDX(i, j, n)] = s;
    }
  }
}

/* ---- recursive D&C (CLRS 4.2) ---- */

static long long *mat_alloc(size_t n) {
  return clrs_xcalloc(n * n, sizeof(long long));
}

static void sub_copy(long long *dst, size_t dm, const long long *src, size_t n,
                     size_t r0, size_t c0, size_t m) {
  CLRS_UNUSED(dm);
  for (size_t i = 0; i < m; i++) {
    for (size_t j = 0; j < m; j++) {
      dst[IDX(i, j, m)] = src[IDX(r0 + i, c0 + j, n)];
    }
  }
}

static void sub_paste(long long *dst, size_t n, size_t r0, size_t c0,
                      const long long *src, size_t m) {
  for (size_t i = 0; i < m; i++) {
    for (size_t j = 0; j < m; j++) {
      dst[IDX(r0 + i, c0 + j, n)] = src[IDX(i, j, m)];
    }
  }
}

void matrix_multiply_recursive(long long *C, const long long *A,
                               const long long *B, size_t n) {
  if (n == 0) {
    return;
  }
  if (n == 1) {
    C[0] = A[0] * B[0];
    return;
  }
  if (n % 2 != 0 || n <= 16) {
    matrix_multiply_naive(C, A, B, n);
    return;
  }

  size_t m = n / 2;
  long long *A11 = mat_alloc(m), *A12 = mat_alloc(m);
  long long *A21 = mat_alloc(m), *A22 = mat_alloc(m);
  long long *B11 = mat_alloc(m), *B12 = mat_alloc(m);
  long long *B21 = mat_alloc(m), *B22 = mat_alloc(m);
  long long *C11 = mat_alloc(m), *C12 = mat_alloc(m);
  long long *C21 = mat_alloc(m), *C22 = mat_alloc(m);
  long long *T = mat_alloc(m);

  sub_copy(A11, m, A, n, 0, 0, m);
  sub_copy(A12, m, A, n, 0, m, m);
  sub_copy(A21, m, A, n, m, 0, m);
  sub_copy(A22, m, A, n, m, m, m);
  sub_copy(B11, m, B, n, 0, 0, m);
  sub_copy(B12, m, B, n, 0, m, m);
  sub_copy(B21, m, B, n, m, 0, m);
  sub_copy(B22, m, B, n, m, m, m);

  /* C11 = A11*B11 + A12*B21 */
  matrix_zero(C11, m);
  matrix_multiply_recursive(T, A11, B11, m);
  matrix_add(C11, C11, T, m);
  matrix_multiply_recursive(T, A12, B21, m);
  matrix_add(C11, C11, T, m);

  /* C12 = A11*B12 + A12*B22 */
  matrix_zero(C12, m);
  matrix_multiply_recursive(T, A11, B12, m);
  matrix_add(C12, C12, T, m);
  matrix_multiply_recursive(T, A12, B22, m);
  matrix_add(C12, C12, T, m);

  /* C21 = A21*B11 + A22*B21 */
  matrix_zero(C21, m);
  matrix_multiply_recursive(T, A21, B11, m);
  matrix_add(C21, C21, T, m);
  matrix_multiply_recursive(T, A22, B21, m);
  matrix_add(C21, C21, T, m);

  /* C22 = A21*B12 + A22*B22 */
  matrix_zero(C22, m);
  matrix_multiply_recursive(T, A21, B12, m);
  matrix_add(C22, C22, T, m);
  matrix_multiply_recursive(T, A22, B22, m);
  matrix_add(C22, C22, T, m);

  sub_paste(C, n, 0, 0, C11, m);
  sub_paste(C, n, 0, m, C12, m);
  sub_paste(C, n, m, 0, C21, m);
  sub_paste(C, n, m, m, C22, m);

  free(A11); free(A12); free(A21); free(A22);
  free(B11); free(B12); free(B21); free(B22);
  free(C11); free(C12); free(C21); free(C22);
  free(T);
}

/* ---- Strassen (CLRS 4.2), n power of 2 ---- */

static void strassen_rec(long long *C, const long long *A, const long long *B,
                         size_t n) {
  if (n == 1) {
    C[0] = A[0] * B[0];
    return;
  }
  if (n <= 32) {
    matrix_multiply_naive(C, A, B, n);
    return;
  }

  size_t m = n / 2;
  long long *A11 = mat_alloc(m), *A12 = mat_alloc(m);
  long long *A21 = mat_alloc(m), *A22 = mat_alloc(m);
  long long *B11 = mat_alloc(m), *B12 = mat_alloc(m);
  long long *B21 = mat_alloc(m), *B22 = mat_alloc(m);
  long long *S1 = mat_alloc(m), *S2 = mat_alloc(m);
  long long *S3 = mat_alloc(m), *S4 = mat_alloc(m);
  long long *S5 = mat_alloc(m), *S6 = mat_alloc(m);
  long long *S7 = mat_alloc(m), *S8 = mat_alloc(m);
  long long *S9 = mat_alloc(m), *S10 = mat_alloc(m);
  long long *P1 = mat_alloc(m), *P2 = mat_alloc(m);
  long long *P3 = mat_alloc(m), *P4 = mat_alloc(m);
  long long *P5 = mat_alloc(m), *P6 = mat_alloc(m);
  long long *P7 = mat_alloc(m);
  long long *T = mat_alloc(m);

  sub_copy(A11, m, A, n, 0, 0, m);
  sub_copy(A12, m, A, n, 0, m, m);
  sub_copy(A21, m, A, n, m, 0, m);
  sub_copy(A22, m, A, n, m, m, m);
  sub_copy(B11, m, B, n, 0, 0, m);
  sub_copy(B12, m, B, n, 0, m, m);
  sub_copy(B21, m, B, n, m, 0, m);
  sub_copy(B22, m, B, n, m, m, m);

  matrix_sub(S1, B12, B22, m);
  matrix_add(S2, A11, A12, m);
  matrix_add(S3, A21, A22, m);
  matrix_sub(S4, B21, B11, m);
  matrix_add(S5, A11, A22, m);
  matrix_add(S6, B11, B22, m);
  matrix_sub(S7, A12, A22, m);
  matrix_add(S8, B21, B22, m);
  matrix_sub(S9, A11, A21, m);
  matrix_add(S10, B11, B12, m);

  strassen_rec(P1, A11, S1, m);
  strassen_rec(P2, S2, B22, m);
  strassen_rec(P3, S3, B11, m);
  strassen_rec(P4, A22, S4, m);
  strassen_rec(P5, S5, S6, m);
  strassen_rec(P6, S7, S8, m);
  strassen_rec(P7, S9, S10, m);

  /* C11 = P5 + P4 - P2 + P6 */
  matrix_add(T, P5, P4, m);
  matrix_sub(T, T, P2, m);
  matrix_add(T, T, P6, m);
  sub_paste(C, n, 0, 0, T, m);

  /* C12 = P1 + P2 */
  matrix_add(T, P1, P2, m);
  sub_paste(C, n, 0, m, T, m);

  /* C21 = P3 + P4 */
  matrix_add(T, P3, P4, m);
  sub_paste(C, n, m, 0, T, m);

  /* C22 = P5 + P1 - P3 - P7 */
  matrix_add(T, P5, P1, m);
  matrix_sub(T, T, P3, m);
  matrix_sub(T, T, P7, m);
  sub_paste(C, n, m, m, T, m);

  free(A11); free(A12); free(A21); free(A22);
  free(B11); free(B12); free(B21); free(B22);
  free(S1); free(S2); free(S3); free(S4); free(S5);
  free(S6); free(S7); free(S8); free(S9); free(S10);
  free(P1); free(P2); free(P3); free(P4); free(P5); free(P6); free(P7);
  free(T);
}

void matrix_multiply_strassen(long long *C, const long long *A,
                              const long long *B, size_t n) {
  CLRS_ASSERT(n > 0 && (n & (n - 1)) == 0,
              "Strassen requires n to be a power of 2");
  matrix_zero(C, n);
  strassen_rec(C, A, B, n);
}
