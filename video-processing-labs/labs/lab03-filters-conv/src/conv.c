#include "conv.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"

void conv_direct(const double *x, int n, const double *h, int m, double *y) {
    int ly = n + m - 1;
    for (int i = 0; i < ly; i++) y[i] = 0.0;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < m; k++) {
            y[i + k] += x[i] * h[k];
        }
    }
}

static int next_pow2(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

/* Iterative radix-2 FFT (in-lab evolution of the recursive lab02 FFT): at
 * N=131072 the recursive version's per-level mallocs dominate the whole
 * convolution time budget. */
static void fft_iter(cpx *a, int n, int inverse) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { cpx t = a[i]; a[i] = a[j]; a[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = (inverse ? 2.0 : -2.0) * M_PI / len;
        double wr = cos(ang), wi = sin(ang);
        for (int i = 0; i < n; i += len) {
            double cr = 1.0, ci = 0.0;
            for (int k = 0; k < len / 2; k++) {
                cpx u = a[i + k];
                cpx *v = &a[i + k + len / 2];
                double vr = v->re * cr - v->im * ci;
                double vi = v->re * ci + v->im * cr;
                v->re = u.re - vr; v->im = u.im - vi;
                a[i + k].re = u.re + vr; a[i + k].im = u.im + vi;
                double ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
    if (inverse) {
        double s = 1.0 / n;
        for (int i = 0; i < n; i++) { a[i].re *= s; a[i].im *= s; }
    }
}

void conv_fft(const double *x, int n, const double *h, int m, double *y) {
    int ly = n + m - 1;
    int N = next_pow2(ly);
    cpx *X = (cpx *)calloc((size_t)N, sizeof(cpx));
    cpx *H = (cpx *)calloc((size_t)N, sizeof(cpx));
    if (!X || !H) {
        free(X);
        free(H);
        return;
    }
    for (int i = 0; i < n; i++) {
        X[i].re = x[i];
        X[i].im = 0.0;
    }
    for (int i = 0; i < m; i++) {
        H[i].re = h[i];
        H[i].im = 0.0;
    }
    fft_iter(X, N, 0);
    fft_iter(H, N, 0);
    for (int i = 0; i < N; i++) {
        double re = X[i].re * H[i].re - X[i].im * H[i].im;
        double im = X[i].re * H[i].im + X[i].im * H[i].re;
        X[i].re = re;
        X[i].im = im;
    }
    fft_iter(X, N, 1);
    for (int i = 0; i < ly; i++) y[i] = X[i].re;
    free(X);
    free(H);
}

double rel_max_err_vec(const double *a, const double *b, int n) {
    double max_b = 1e-300, max_e = 0.0;
    for (int i = 0; i < n; i++) {
        double bm = fabs(b[i]);
        if (bm > max_b) max_b = bm;
        double e = fabs(a[i] - b[i]);
        if (e > max_e) max_e = e;
    }
    return max_e / max_b;
}
