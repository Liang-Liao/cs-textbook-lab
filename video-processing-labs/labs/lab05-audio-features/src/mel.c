#include "mel.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

double hz_to_mel(double f_hz) {
    if (f_hz < 0.0) f_hz = 0.0;
    return 2595.0 * log10(1.0 + f_hz / 700.0);
}

double mel_to_hz(double mel) { return 700.0 * (pow(10.0, mel / 2595.0) - 1.0); }

void mel_filterbank(int n_mels, int nfft, double fs, double fmin, double fmax,
                    double *weights, double *centers_hz) {
    if (n_mels <= 0 || nfft <= 0 || !weights || !centers_hz) return;
    int n_bins = nfft / 2 + 1;
    memset(weights, 0, (size_t)n_mels * (size_t)n_bins * sizeof(double));

    double mel_min = hz_to_mel(fmin);
    double mel_max = hz_to_mel(fmax);
    /* n_mels + 2 uniformly spaced mel points */
    int n_pts = n_mels + 2;
    double *pts = (double *)malloc((size_t)n_pts * sizeof(double));
    if (!pts) return;
    for (int i = 0; i < n_pts; i++) {
        double m = mel_min + (mel_max - mel_min) * (double)i / (double)(n_pts - 1);
        pts[i] = mel_to_hz(m);
    }

    for (int m = 0; m < n_mels; m++) {
        double lo = pts[m];
        double mid = pts[m + 1];
        double hi = pts[m + 2];
        centers_hz[m] = mid;
        double *w = weights + (size_t)m * (size_t)n_bins;
        for (int k = 0; k < n_bins; k++) {
            double f = (double)k * fs / (double)nfft;
            if (f <= lo || f >= hi) {
                w[k] = 0.0;
            } else if (f <= mid) {
                w[k] = (f - lo) / (mid - lo);
            } else {
                w[k] = (hi - f) / (hi - mid);
            }
        }
    }
    free(pts);
}

void mel_spectrogram(const double *power, int n_frames, int n_bins,
                     const double *weights, int n_mels, double *mel_pow) {
    if (!power || !weights || !mel_pow) return;
    for (int t = 0; t < n_frames; t++) {
        const double *P = power + (size_t)t * (size_t)n_bins;
        double *M = mel_pow + (size_t)t * (size_t)n_mels;
        for (int m = 0; m < n_mels; m++) {
            const double *w = weights + (size_t)m * (size_t)n_bins;
            double acc = 0.0;
            for (int k = 0; k < n_bins; k++) acc += P[k] * w[k];
            M[m] = acc;
        }
    }
}

void mel_to_pgm(const double *mel_pow_db, int n_frames, int n_mels, uint8_t *img,
                int w, int h) {
    if (!mel_pow_db || !img || w <= 0 || h <= 0) return;
    double lo = 1e300, hi = -1e300;
    for (int t = 0; t < n_frames; t++) {
        const double *row = mel_pow_db + (size_t)t * (size_t)n_mels;
        for (int m = 0; m < n_mels; m++) {
            if (row[m] < lo) lo = row[m];
            if (row[m] > hi) hi = row[m];
        }
    }
    if (hi - lo < 1e-9) hi = lo + 1.0;
    for (int r = 0; r < h; r++) {
        /* row 0 = highest mel band */
        float tm = (float)r / (float)(h > 1 ? h - 1 : 1);
        int m = (int)lrint((1.0 - (double)tm) * (double)(n_mels - 1));
        if (m < 0) m = 0;
        if (m >= n_mels) m = n_mels - 1;
        for (int c = 0; c < w; c++) {
            int t = (int)((long long)c * (n_frames - 1) / (w > 1 ? w - 1 : 1));
            if (t < 0) t = 0;
            if (t >= n_frames) t = n_frames - 1;
            double v = mel_pow_db[(size_t)t * (size_t)n_mels + m];
            double u = (v - lo) / (hi - lo);
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            img[r * w + c] = (uint8_t)lrint(u * 255.0);
        }
    }
}
