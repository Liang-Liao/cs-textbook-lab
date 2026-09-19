#ifndef LAB05_MEL_H
#define LAB05_MEL_H

#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

double hz_to_mel(double f_hz);
double mel_to_hz(double mel);

/* Build n_mels triangular filters over [fmin, fmax].
 * bin_hz[k] = k * fs / nfft for k in [0, n_bins).
 * weights are dense per filter: n_bins doubles each (0 outside support).
 * centers_hz receives n_mels center frequencies. */
void mel_filterbank(int n_mels, int nfft, double fs, double fmin, double fmax,
                    double *weights, double *centers_hz);

/* Power spectrogram (n_frames x n_bins) -> mel power (n_frames x n_mels). */
void mel_spectrogram(const double *power, int n_frames, int n_bins,
                     const double *weights, int n_mels, double *mel_pow);

/* Write mel spectrogram as PGM heatmap (row0 = highest mel band). */
void mel_to_pgm(const double *mel_pow_db, int n_frames, int n_mels, uint8_t *img,
                int w, int h);

#endif
