#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "poly_fft.h"
#include "test.h"

static int almost_eq(double a, double b, double eps) {
  double d = a - b;
  if (d < 0) {
    d = -d;
  }
  return d <= eps;
}

static void check_poly(TestSuite *t, const double *A, const double *B,
                       size_t n) {
  double *naive = clrs_xmalloc(2 * n * sizeof(double));
  double *fftm = clrs_xmalloc(2 * n * sizeof(double));
  poly_multiply_naive(A, B, n, naive);
  poly_multiply_fft(A, B, n, fftm);
  for (size_t i = 0; i < 2 * n; i++) {
    if (!almost_eq(naive[i], fftm[i], 1e-6)) {
      fprintf(stderr, "  mismatch[%zu]: naive=%f fft=%f\n", i, naive[i],
              fftm[i]);
    }
    ASSERT_TRUE(t, almost_eq(naive[i], fftm[i], 1e-6));
  }
  free(naive);
  free(fftm);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* (1 + x) * (1 + x) = 1 + 2x + x^2 */
  {
    double A[] = {1, 1};
    double B[] = {1, 1};
    double out[4];
    poly_multiply_fft(A, B, 2, out);
    ASSERT_TRUE(&t, almost_eq(out[0], 1, 1e-9));
    ASSERT_TRUE(&t, almost_eq(out[1], 2, 1e-9));
    ASSERT_TRUE(&t, almost_eq(out[2], 1, 1e-9));
    ASSERT_TRUE(&t, almost_eq(out[3], 0, 1e-9));
  }

  /* (1+2x+3x^2) * (4+5x+6x^2) with n=4 pad */
  {
    double A[] = {1, 2, 3, 0};
    double B[] = {4, 5, 6, 0};
    check_poly(&t, A, B, 4);
    double out[8];
    poly_multiply_fft(A, B, 4, out);
    /* c0=4, c1=5+8=13, c2=6+10+12=28, c3=12+15=27, c4=18 */
    ASSERT_TRUE(&t, almost_eq(out[0], 4, 1e-6));
    ASSERT_TRUE(&t, almost_eq(out[1], 13, 1e-6));
    ASSERT_TRUE(&t, almost_eq(out[2], 28, 1e-6));
    ASSERT_TRUE(&t, almost_eq(out[3], 27, 1e-6));
    ASSERT_TRUE(&t, almost_eq(out[4], 18, 1e-6));
  }

  /* random polynomials */
  {
    size_t n = 16;
    double A[16], B[16];
    srand(42u);
    for (size_t i = 0; i < n; i++) {
      A[i] = (double)(rand() % 21 - 10);
      B[i] = (double)(rand() % 21 - 10);
    }
    check_poly(&t, A, B, n);
  }

  {
    size_t n = 32;
    double A[32], B[32];
    srand(7u);
    for (size_t i = 0; i < n; i++) {
      A[i] = (double)(rand() % 11);
      B[i] = (double)(rand() % 11);
    }
    check_poly(&t, A, B, n);
  }

  /* FFT identity: forward then inverse recovers */
  {
    size_t n = 8;
    double re[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    double im[8] = {0};
    double orig[8];
    for (size_t i = 0; i < n; i++) {
      orig[i] = re[i];
    }
    fft(re, im, n, 0);
    fft(re, im, n, 1);
    for (size_t i = 0; i < n; i++) {
      if (!almost_eq(re[i], orig[i], 1e-9) || !almost_eq(im[i], 0, 1e-9)) {
        fprintf(stderr, "  fft roundtrip[%zu]: %f (want %f)\n", i, re[i],
                orig[i]);
      }
      ASSERT_TRUE(&t, almost_eq(re[i], orig[i], 1e-9));
      ASSERT_TRUE(&t, almost_eq(im[i], 0, 1e-9));
    }
  }

  /* constant * linear */
  {
    double A[] = {3, 0, 0, 0};
    double B[] = {0, 2, 0, 0};
    double out[8];
    poly_multiply_fft(A, B, 4, out);
    ASSERT_TRUE(&t, almost_eq(out[1], 6, 1e-9));
  }

  return test_report(&t, "poly_fft");
}
