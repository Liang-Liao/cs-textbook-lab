#include "mdct.h"
#include <math.h>
#include <stdlib.h>

void mdct(const double *x, double *X, int N) {
    int n = N / 2;
    for (int k = 0; k < n; k++) {
        double s = 0.0;
        for (int i = 0; i < N; i++) {
            s += x[i] * cos(M_PI / n * (i + 0.5 + n * 0.5) * (k + 0.5));
        }
        X[k] = s;
    }
}

void imdct(const double *X, double *x, int N) {
    int n = N / 2;
    for (int i = 0; i < N; i++) {
        double s = 0.0;
        for (int k = 0; k < n; k++) {
            s += X[k] * cos(M_PI / n * (i + 0.5 + n * 0.5) * (k + 0.5));
        }
        x[i] = 2.0 * s / n; /* scale for TDAC + sine window 50% OLA */
    }
}

static void sine_window(double *w, int N) {
    for (int i = 0; i < N; i++)
        w[i] = sin(M_PI / N * (i + 0.5));
}

double mdct_roundtrip_err(const double *sig, int n, int N) {
    int hop = N / 2;
    int nframes = 1 + (n - N) / hop;
    if (nframes < 1) return 1.0;
    double *w = malloc((size_t)N * sizeof(double));
    double *x = calloc((size_t)N, sizeof(double));
    double *X = calloc((size_t)hop, sizeof(double));
    double *y = calloc((size_t)N, sizeof(double));
    double *out = calloc((size_t)n, sizeof(double));
    if (!w || !x || !X || !y || !out) {
        free(w); free(x); free(X); free(y); free(out);
        return 1.0;
    }
    sine_window(w, N);
    for (int f = 0; f < nframes; f++) {
        int s0 = f * hop;
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            x[i] = ((idx >= 0 && idx < n) ? sig[idx] : 0.0) * w[i];
        }
        mdct(x, X, N);
        imdct(X, y, N);
        for (int i = 0; i < N; i++) {
            int idx = s0 + i;
            if (idx >= 0 && idx < n) out[idx] += y[i] * w[i];
        }
    }
    /* compare interior */
    double e = 0, r = 0;
    for (int i = N; i < n - N; i++) {
        double d = out[i] - sig[i];
        e += d * d;
        r += sig[i] * sig[i];
    }
    free(w); free(x); free(X); free(y); free(out);
    if (r <= 0) return 1.0;
    return sqrt(e / r);
}
