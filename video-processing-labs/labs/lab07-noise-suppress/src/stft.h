/* copied from lab02-time-frequency */
#ifndef LAB07_STFT_H
#define LAB07_STFT_H
#include "fft.h"
void hann(double *w, int n);
void stft_analyze(const double *sig, int n_sig, int nfft, int hop, cpx *frames, int *nf);
void stft_synthesize(const cpx *frames, int nf, int nfft, int hop, double *sig, int n_sig);
#endif
