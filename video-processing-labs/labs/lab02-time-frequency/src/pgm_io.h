/* copied from lab01-sampling-quantization */
#ifndef LAB02_PGM_IO_H
#define LAB02_PGM_IO_H

#include <stdint.h>

int pgm_write(const char *path, const uint8_t *pixels, int w, int h);

/* Draw magnitude spectrogram (dB) from signal using FFT. row0 = high freq. */
void spectrogram_to_pgm(const float *sig, int n_sig, int nfft, int hop,
                        uint8_t *buf, int w, int h);

#endif
