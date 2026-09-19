#ifndef LAB07_NS_H
#define LAB07_NS_H
#include <stdint.h>
#include "fft.h"

/* Decision-directed Wiener gain per bin given prior/post SNR. */
void dd_wiener_gain(const double *Y2, const double *N2, double *xi, int n, double *G);
/* Spectral subtraction magnitude. */
void spectral_subtract_mag(double *mag, const double *Nmag, int n, double alpha, double beta);

/* Min-statistics noise tracker (simplified rectangular window min). */
typedef struct {
    int n; int win; int pos;
    double *buf; /* win * n circular */
    double *N2;
} minstat;
void minstat_init(minstat *m, int n, int win);
void minstat_free(minstat *m);
void minstat_update(minstat *m, const double *Y2);

/* Estimate noise PSD from noisy signal only: first n_init frames then minstat. */
void ns_estimate_noise(const double *noisy, int n, int nfft, int hop,
                       int n_init, double *N2);

/* Frame NS with fixed N2 (no clean reference). */
void ns_process_frames(cpx *frames, int nf, int nfft, double *N2, double floor_g);

/* Spectral subtraction over frames using noise power N2. */
void ns_process_spectral_sub(cpx *frames, int nf, int nfft, const double *N2,
                             double alpha, double beta);

/* Simplified WebRTC-style online NS:
 * minstat noise (updated on non-speech frames only) + per-bin likelihood-ratio
 * speech decision + DD prior SNR + Wiener + floor.
 * Returns number of frames classified as speech. */
int ns_process_webrtc(cpx *frames, int nf, int nfft, double floor_g);

double snr_db(const double *ref, const double *test, int n);

/* Speech-bin gain damage (dB) of out vs noisy on speech-dominant bins. */
double speech_damage_db(const double *clean, const double *noisy, const double *out,
                        int n, int nfft, int hop);

/* dB magnitude spectrogram heatmap; row 0 = high frequency. */
void spectrogram_to_pgm(const double *sig, int n, int nfft, int hop,
                        uint8_t *buf, int w, int h);
#endif
