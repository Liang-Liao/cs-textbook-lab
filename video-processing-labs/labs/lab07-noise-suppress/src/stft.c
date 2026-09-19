/* copied from lab02-time-frequency (adapted for lab07-noise-suppress) */
#include "stft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
void hann(double *w, int n) {
    for (int i = 0; i < n; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * i / n);
}
void stft_analyze(const double *sig, int n_sig, int nfft, int hop, cpx *frames, int *nf) {
    double *w = malloc((size_t)nfft * sizeof(double));
    if (!w) { if (nf) *nf = 0; return; }
    hann(w, nfft);
    int fr = 1 + (n_sig - nfft) / hop;
    if (fr < 1) fr = 1;
    for (int f = 0; f < fr; f++) {
        cpx *X = frames + (size_t)f * nfft;
        int s0 = f * hop;
        for (int i = 0; i < nfft; i++) {
            int idx = s0 + i;
            X[i].re = ((idx >= 0 && idx < n_sig) ? sig[idx] : 0.0) * w[i];
            X[i].im = 0.0;
        }
        fft(X, nfft);
    }
    if (nf) *nf = fr;
    free(w);
}
void stft_synthesize(const cpx *frames, int nf, int nfft, int hop, double *sig, int n_sig) {
    memset(sig, 0, (size_t)n_sig * sizeof(double));
    cpx *t = malloc((size_t)nfft * sizeof(cpx));
    if (!t) return;
    for (int f = 0; f < nf; f++) {
        memcpy(t, frames + (size_t)f * nfft, (size_t)nfft * sizeof(cpx));
        ifft(t, nfft);
        int s0 = f * hop;
        for (int i = 0; i < nfft; i++) {
            int idx = s0 + i;
            if (idx >= 0 && idx < n_sig) sig[idx] += t[i].re;
        }
    }
    free(t);
}
