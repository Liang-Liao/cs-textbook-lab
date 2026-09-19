#include "pgm_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dsp_tools.h"

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

void pgm_spectrogram(const float *sig, size_t n, int nfft, int hop,
                     uint8_t *buf, int w, int h) {
    if (!sig || !buf || nfft <= 0 || hop <= 0 || w <= 0 || h <= 0) return;
    float *win = (float *)malloc((size_t)nfft * sizeof(float));
    float *frame = (float *)malloc((size_t)nfft * sizeof(float));
    float *mag = (float *)malloc((size_t)nfft * sizeof(float));
    if (!win || !frame || !mag) {
        free(win);
        free(frame);
        free(mag);
        return;
    }
    for (int i = 0; i < nfft; i++) {
        win[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (float)(nfft - 1));
    }

    for (int col = 0; col < w; col++) {
        size_t start = (size_t)col * (size_t)hop;
        for (int i = 0; i < nfft; i++) {
            size_t idx = start + (size_t)i;
            frame[i] = (idx < n) ? sig[idx] * win[i] : 0.0f;
        }
        dft_magnitude(frame, nfft, mag);
        int bins = nfft / 2;
        for (int row = 0; row < h; row++) {
            /* row 0 = high frequency (top), row h-1 = DC (bottom) */
            float t = (float)row / (float)(h - 1);
            int b = (int)lrintf((1.0f - t) * (float)(bins - 1));
            if (b < 0) b = 0;
            if (b >= bins) b = bins - 1;
            float m = mag[b] / (float)nfft;
            float db = 20.0f * log10f(m + 1e-12f);
            float u = (db + 80.0f) / 80.0f; /* -80..0 dB → 0..1 */
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            buf[row * w + col] = (uint8_t)lrintf(u * 255.0f);
        }
    }
    free(win);
    free(frame);
    free(mag);
}
