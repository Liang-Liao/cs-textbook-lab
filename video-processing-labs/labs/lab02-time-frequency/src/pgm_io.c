/* copied from lab01-sampling-quantization (adapted for lab02-time-frequency) */
#include "pgm_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "fft.h"
#include "stft.h"

int pgm_write(const char *path, const uint8_t *pixels, int w, int h) {
    if (!path || !pixels || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(pixels, 1, (size_t)w * (size_t)h, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}

void spectrogram_to_pgm(const float *sig, int n_sig, int nfft, int hop,
                        uint8_t *buf, int w, int h) {
    if (!sig || !buf || nfft <= 0 || hop <= 0 || w <= 0 || h <= 0) return;
    double *win = (double *)malloc((size_t)nfft * sizeof(double));
    cpx *frame = (cpx *)malloc((size_t)nfft * sizeof(cpx));
    if (!win || !frame) {
        free(win);
        free(frame);
        return;
    }
    hann_window(win, nfft);
    int bins = nfft / 2;
    for (int col = 0; col < w; col++) {
        int start = (int)((long long)col * (n_sig - nfft) / (w > 1 ? w - 1 : 1));
        if (start < 0) start = 0;
        for (int i = 0; i < nfft; i++) {
            int idx = start + i;
            double s = (idx >= 0 && idx < n_sig) ? (double)sig[idx] : 0.0;
            frame[i].re = s * win[i];
            frame[i].im = 0.0;
        }
        fft(frame, nfft);
        for (int row = 0; row < h; row++) {
            float t = (float)row / (float)(h > 1 ? h - 1 : 1);
            int b = (int)lrint((1.0 - (double)t) * (double)(bins - 1));
            if (b < 0) b = 0;
            if (b >= bins) b = bins - 1;
            double m = sqrt(frame[b].re * frame[b].re + frame[b].im * frame[b].im) /
                       (double)nfft;
            double db = 20.0 * log10(m + 1e-12);
            double u = (db + 80.0) / 80.0;
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            buf[row * w + col] = (uint8_t)lrint(u * 255.0);
        }
    }
    free(win);
    free(frame);
}
