/* copied from lab02-time-frequency */
#ifndef LAB05_STFT_H
#define LAB05_STFT_H

#include "fft.h"

void hann_window(double *w, int n);

/* Analyze into full-nfft complex frames (Hann window). */
void stft_analyze(const double *sig, int n_sig, int nfft, int hop, cpx *frames,
                  int *n_frames);

#endif
