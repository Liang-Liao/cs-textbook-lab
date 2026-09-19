#include "stft.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void hann_window(double *w, int n) {
    if (n <= 0) return;
    if (n == 1) {
        w[0] = 1.0;
        return;
    }
    for (int i = 0; i < n; i++) {
        w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)n);
    }
}

void stft_analyze(const double *sig, int n_sig, int nfft, int hop,
                  cpx *frames, int *n_frames) {
    if (!sig || !frames || nfft <= 0 || hop <= 0) {
        if (n_frames) *n_frames = 0;
        return;
    }
    double *w = (double *)malloc((size_t)nfft * sizeof(double));
    if (!w) {
        if (n_frames) *n_frames = 0;
        return;
    }
    hann_window(w, nfft);
    int frames_n = 1 + (n_sig - nfft) / hop;
    if (frames_n < 1) frames_n = 1;
    for (int f = 0; f < frames_n; f++) {
        cpx *X = frames + (size_t)f * (size_t)nfft;
        int start = f * hop;
        for (int i = 0; i < nfft; i++) {
            int idx = start + i;
            double s = (idx >= 0 && idx < n_sig) ? sig[idx] : 0.0;
            X[i].re = s * w[i];
            X[i].im = 0.0;
        }
        fft(X, nfft);
    }
    if (n_frames) *n_frames = frames_n;
    free(w);
}

void stft_synthesize(const cpx *frames, int n_frames, int nfft, int hop,
                     double *sig, int n_sig) {
    if (!frames || !sig || nfft <= 0 || hop <= 0) return;
    memset(sig, 0, (size_t)n_sig * sizeof(double));
    cpx *tmp = (cpx *)malloc((size_t)nfft * sizeof(cpx));
    if (!tmp) return;
    for (int f = 0; f < n_frames; f++) {
        memcpy(tmp, frames + (size_t)f * (size_t)nfft, (size_t)nfft * sizeof(cpx));
        ifft_rec(tmp, nfft);
        int start = f * hop;
        for (int i = 0; i < nfft; i++) {
            int idx = start + i;
            if (idx >= 0 && idx < n_sig) sig[idx] += tmp[i].re;
        }
    }
    free(tmp);
}
