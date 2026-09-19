#include "conv2d.h"

#include <math.h>

static double sample_rep(const double *src, int w, int h, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    return src[y * w + x];
}

int conv2d_direct(const double *src, int w, int h, const double *k, int kw, int kh,
                  double *dst) {
    if (!src || !k || !dst || w <= 0 || h <= 0 || kw <= 0 || kh <= 0) return -1;
    int ox = kw / 2;
    int oy = kh / 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double acc = 0.0;
            for (int j = 0; j < kh; j++) {
                for (int i = 0; i < kw; i++) {
                    acc += k[j * kw + i] * sample_rep(src, w, h, x + i - ox, y + j - oy);
                }
            }
            dst[y * w + x] = acc;
        }
    }
    return 0;
}

int conv2d_separable(const double *src, int w, int h, const double *kx, int klen,
                     const double *ky, double *tmp, double *dst) {
    if (!src || !kx || !ky || !tmp || !dst || w <= 0 || h <= 0 || klen <= 0) return -1;
    int o = klen / 2;
    /* horizontal pass */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double acc = 0.0;
            for (int i = 0; i < klen; i++) {
                acc += kx[i] * sample_rep(src, w, h, x + i - o, y);
            }
            tmp[y * w + x] = acc;
        }
    }
    /* vertical pass */
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double acc = 0.0;
            for (int j = 0; j < klen; j++) {
                acc += ky[j] * sample_rep(tmp, w, h, x, y + j - o);
            }
            dst[y * w + x] = acc;
        }
    }
    return 0;
}

void gauss_kernel_1d(double *k, int n, double sigma) {
    int o = n / 2;
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        double x = (double)(i - o);
        k[i] = exp(-0.5 * x * x / (sigma * sigma));
        sum += k[i];
    }
    for (int i = 0; i < n; i++) k[i] /= sum;
}

void gauss_kernel_2d(double *k, int n, double sigma) {
    double *t = k;
    int o = n / 2;
    double sum = 0.0;
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            double x = (double)(i - o);
            double y = (double)(j - o);
            t[j * n + i] = exp(-0.5 * (x * x + y * y) / (sigma * sigma));
            sum += t[j * n + i];
        }
    }
    for (int i = 0; i < n * n; i++) k[i] /= sum;
}

double rel_max_err(const double *a, const double *b, int n) {
    double num = 0.0, den = 0.0;
    for (int i = 0; i < n; i++) {
        double d = fabs(a[i] - b[i]);
        if (d > num) num = d;
        double m = fabs(a[i]);
        if (fabs(b[i]) > m) m = fabs(b[i]);
        if (m > den) den = m;
    }
    if (den < 1e-12) den = 1.0;
    return num / den;
}
