#include "poly_fft.h"

#include <math.h>
#include <stdlib.h>

#include "clrs.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void poly_multiply_naive(const double *A, const double *B, size_t n,
                         double *out) {
  for (size_t i = 0; i < 2 * n; i++) {
    out[i] = 0.0;
  }
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      out[i + j] += A[i] * B[j];
    }
  }
}

/* In-place iterative radix-2 FFT */
void fft(double *re, double *im, size_t n, int inverse) {
  CLRS_ASSERT(n > 0 && (n & (n - 1)) == 0, "n must be power of 2");

  /* bit-reversal permutation */
  for (size_t i = 1, j = 0; i < n; i++) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      double tr = re[i];
      re[i] = re[j];
      re[j] = tr;
      double ti = im[i];
      im[i] = im[j];
      im[j] = ti;
    }
  }

  for (size_t len = 2; len <= n; len <<= 1) {
    double ang = 2.0 * M_PI / (double)len * (inverse ? 1.0 : -1.0);
    double wlen_re = cos(ang);
    double wlen_im = sin(ang);
    for (size_t i = 0; i < n; i += len) {
      double w_re = 1.0, w_im = 0.0;
      for (size_t j = 0; j < len / 2; j++) {
        size_t u = i + j;
        size_t v = i + j + len / 2;
        double vr = re[v] * w_re - im[v] * w_im;
        double vi = re[v] * w_im + im[v] * w_re;
        re[v] = re[u] - vr;
        im[v] = im[u] - vi;
        re[u] += vr;
        im[u] += vi;
        double nwr = w_re * wlen_re - w_im * wlen_im;
        w_im = w_re * wlen_im + w_im * wlen_re;
        w_re = nwr;
      }
    }
  }

  if (inverse) {
    for (size_t i = 0; i < n; i++) {
      re[i] /= (double)n;
      im[i] /= (double)n;
    }
  }
}

void poly_multiply_fft(const double *A, const double *B, size_t n,
                       double *out) {
  CLRS_ASSERT(n > 0 && (n & (n - 1)) == 0, "n must be power of 2");
  size_t m = 2 * n;
  double *ar = clrs_xcalloc(m, sizeof(double));
  double *ai = clrs_xcalloc(m, sizeof(double));
  double *br = clrs_xcalloc(m, sizeof(double));
  double *bi = clrs_xcalloc(m, sizeof(double));

  for (size_t i = 0; i < n; i++) {
    ar[i] = A[i];
    br[i] = B[i];
  }

  fft(ar, ai, m, 0);
  fft(br, bi, m, 0);

  for (size_t i = 0; i < m; i++) {
    double r = ar[i] * br[i] - ai[i] * bi[i];
    double im = ar[i] * bi[i] + ai[i] * br[i];
    ar[i] = r;
    ai[i] = im;
  }

  fft(ar, ai, m, 1);

  for (size_t i = 0; i < m; i++) {
    out[i] = ar[i];
  }

  free(ar);
  free(ai);
  free(br);
  free(bi);
}
