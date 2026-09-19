#ifndef LAB16_CODEC_H
#define LAB16_CODEC_H

#include <stdint.h>

#include "psycho.h"

/* Perceptual MDCT encode→quantize(by SMR)→Huffman→decode (closed loop).
 * recon may be NULL. Returns fraction of (frame,band) pairs with quant noise
 * energy below the masking threshold. Also reports SNR and mean bits/sample.
 * If collect is non-zero, fills out_spec/out_thr/out_noise as n_frames x n_bins
 * dB arrays (caller frees). */
double perceptual_process(const double *sig, int n, int N, double fs, double *recon,
                          double *out_snr_db, double *out_bits_per_sample,
                          int *out_n_checked, int collect, double **out_spec,
                          double **out_thr, double **out_noise, int *out_n_frames);

/* Fixed-bit uniform MDCT quantizer (contrast). Returns time-domain SNR dB. */
double uniform_mdct_snr(const double *sig, int n, int N, double fs, int bits,
                        double *recon, double *out_bits_per_sample);

/* ADPCM 4-bit path: time-domain SNR + fraction of Bark bands where error
 * energy stays under the STFT masking threshold of the reference. */
double adpcm_noise_frac(const double *sig, int n, double fs, double *out_snr_db);

/* Uniform-MDCT noise fraction under STFT masking thr (equal-rate contrast). */
double uniform_noise_frac(const double *sig, int n, int N, double fs, int bits,
                          double *out_snr_db);

int write_noise_pgm(const char *path, const double *spec_db, const double *thr_db,
                    const double *noise_db, int n_frames, int n_bins);

#endif
