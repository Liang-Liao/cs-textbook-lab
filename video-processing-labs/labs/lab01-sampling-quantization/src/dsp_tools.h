#ifndef LAB01_DSP_TOOLS_H
#define LAB01_DSP_TOOLS_H

#include <stddef.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Multi-tone: sum of sines with amps/freqs, full-scale normalized to peak 0.9. */
void synth_multitone(float *out, size_t n, double fs, const double *freqs,
                     const double *amps, int n_tones);

/* Single sine at given peak amplitude (use ~1.0 for full-scale SNR theory). */
void synth_sine(float *out, size_t n, double fs, double freq, double peak_amp);

/* Quantize float [-1,1) to B-bit symmetric mid-tread, return SNR in dB. */
double quantize_snr_db(const float *x, size_t n, int bits, float *y_out);

/* Double-precision SNR (preferred for theory comparison). */
double quantize_snr_db_double(const double *x, size_t n, int bits);

/* Uniform dither noise amplitude (TPDF) about 1 LSB peak-to-peak half. */
void apply_dither(float *x, size_t n, double fs, int bits, unsigned seed);

/* Naive DFT magnitude (linear) for nfft bins on window of x. */
void dft_magnitude(const float *x, int nfft, float *mag);

double rms(const float *x, size_t n);
double peak_abs(const float *x, size_t n);

/* Theoretical quantization SNR for full-scale sine. */
double quant_snr_theory_db(int bits);

/* Alias fold of freq into [0, fs/2]. */
double alias_fold(double freq, double fs);

#endif
