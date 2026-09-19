#ifndef LAB02_STFT_H
#define LAB02_STFT_H

#include "fft.h"

/* Periodic Hann: w[i]=0.5-0.5*cos(2*pi*i/N) — COLA sum=1 at hop=N/2. */
void hann_window(double *w, int n);

/* Analyze double signal into full-nfft complex frames. */
void stft_analyze(const double *sig, int n_sig, int nfft, int hop,
                  cpx *frames, int *n_frames);

/* OLA synthesis (Hann 50%, no second window). */
void stft_synthesize(const cpx *frames, int n_frames, int nfft, int hop,
                     double *sig, int n_sig);

#endif
