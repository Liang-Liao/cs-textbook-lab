#ifndef LAB01_PGM_IO_H
#define LAB01_PGM_IO_H

#include <stddef.h>
#include <stdint.h>

/* Write binary P5 grayscale image. pixels: w*h, 0..255. */
int pgm_write(const char *path, const uint8_t *pixels, int w, int h);

/* Paint a dB-scaled STFT-like magnitude spectrogram heatmap into buf (w*h). */
void pgm_spectrogram(const float *sig, size_t n, int nfft, int hop,
                     uint8_t *buf, int w, int h);

#endif
