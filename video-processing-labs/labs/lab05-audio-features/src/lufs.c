#include "lufs.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* RBJ high-shelf. */
static void bj_highshelf(bq *b, double fs, double f0, double q, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * f0 / fs;
    double cosw = cos(w0), sinw = sin(w0);
    double alpha = sinw / (2.0 * q);
    double beta = 2.0 * sqrt(A) * alpha;
    double b0 = A * ((A + 1.0) + (A - 1.0) * cosw + beta);
    double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
    double b2 = A * ((A + 1.0) + (A - 1.0) * cosw - beta);
    double a0 = (A + 1.0) - (A - 1.0) * cosw + beta;
    double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
    double a2 = (A + 1.0) - (A - 1.0) * cosw - beta;
    b->b0 = b0 / a0;
    b->b1 = b1 / a0;
    b->b2 = b2 / a0;
    b->a1 = a1 / a0;
    b->a2 = a2 / a0;
}

/* RBJ high-pass (RLB shape). */
static void bj_highpass(bq *b, double fs, double f0, double q) {
    double w0 = 2.0 * M_PI * f0 / fs;
    double cosw = cos(w0), sinw = sin(w0);
    double alpha = sinw / (2.0 * q);
    double b0 = (1.0 + cosw) / 2.0;
    double b1 = -(1.0 + cosw);
    double b2 = (1.0 + cosw) / 2.0;
    double a0 = 1.0 + alpha;
    double a1 = -2.0 * cosw;
    double a2 = 1.0 - alpha;
    b->b0 = b0 / a0;
    b->b1 = b1 / a0;
    b->b2 = b2 / a0;
    b->a1 = a1 / a0;
    b->a2 = a2 / a0;
}

void k_weighting_design(bq *shelf, bq *hpf, double fs) {
    /* Approximate BS.1770 pre-filter (high shelf) + RLB high-pass. */
    bj_highshelf(shelf, fs, 1681.974450955533, 0.7071752369554196, 3.999843853973347);
    bj_highpass(hpf, fs, 38.13547087602444, 0.5003270373238773);
}

void bq_apply(const bq *c, const double *x, int n, double *y) {
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    for (int i = 0; i < n; i++) {
        double xi = x[i];
        double yi = c->b0 * xi + c->b1 * x1 + c->b2 * x2 - c->a1 * y1 - c->a2 * y2;
        x2 = x1;
        x1 = xi;
        y2 = y1;
        y1 = yi;
        y[i] = yi;
    }
}

double lufs_integrated(const double *x, int n, double fs) {
    if (!x || n <= 0 || fs <= 0.0) return -120.0;

    bq shelf, hpf;
    k_weighting_design(&shelf, &hpf, fs);

    double *y1 = (double *)malloc((size_t)n * sizeof(double));
    double *y2 = (double *)malloc((size_t)n * sizeof(double));
    if (!y1 || !y2) {
        free(y1);
        free(y2);
        return -120.0;
    }
    bq_apply(&shelf, x, n, y1);
    bq_apply(&hpf, y1, n, y2);

    /* 400 ms block, 75% overlap => hop 100 ms */
    int block = (int)lrint(0.4 * fs);
    int hop = (int)lrint(0.1 * fs);
    if (block < 16) block = 16;
    if (hop < 4) hop = 4;
    if (n < block) {
        free(y1);
        free(y2);
        return -120.0;
    }
    int n_blocks = 1 + (n - block) / hop;

    double *ms = (double *)malloc((size_t)n_blocks * sizeof(double));
    double *lg = (double *)malloc((size_t)n_blocks * sizeof(double));
    if (!ms || !lg) {
        free(y1);
        free(y2);
        free(ms);
        free(lg);
        return -120.0;
    }

    for (int b = 0; b < n_blocks; b++) {
        int start = b * hop;
        double acc = 0.0;
        for (int i = 0; i < block; i++) {
            double v = y2[start + i];
            acc += v * v;
        }
        ms[b] = acc / (double)block;
        lg[b] = -0.691 + 10.0 * log10(ms[b] + 1e-30);
    }

    /* absolute gate -70 LUFS */
    double sum_abs = 0.0;
    int n_abs = 0;
    for (int b = 0; b < n_blocks; b++) {
        if (lg[b] > -70.0) {
            sum_abs += ms[b];
            n_abs++;
        }
    }
    if (n_abs == 0) {
        free(y1);
        free(y2);
        free(ms);
        free(lg);
        return -120.0;
    }
    double mean_abs = sum_abs / (double)n_abs;
    double thr_rel = -0.691 + 10.0 * log10(mean_abs + 1e-30) - 10.0;

    double sum = 0.0;
    int n_kept = 0;
    for (int b = 0; b < n_blocks; b++) {
        if (lg[b] > -70.0 && lg[b] > thr_rel) {
            sum += ms[b];
            n_kept++;
        }
    }
    free(y1);
    free(y2);
    free(ms);
    free(lg);
    if (n_kept == 0) return -120.0;
    return -0.691 + 10.0 * log10(sum / (double)n_kept + 1e-30);
}

double lufs_integrated_gain(const double *x, int n, double fs, double gain) {
    double *y = (double *)malloc((size_t)n * sizeof(double));
    if (!y) return -120.0;
    for (int i = 0; i < n; i++) y[i] = x[i] * gain;
    double l = lufs_integrated(y, n, fs);
    free(y);
    return l;
}
