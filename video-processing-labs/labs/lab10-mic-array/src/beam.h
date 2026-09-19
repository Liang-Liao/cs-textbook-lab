#ifndef LAB10_BEAM_H
#define LAB10_BEAM_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
typedef struct { double re, im; } cpx;
void fft(cpx *d, int n);
void ifft(cpx *d, int n);

/* Cross-correlation lag (y relative to x), subsample via parabolic. */
double xcorr_lag(const double *x, const double *y, int n, int max_lag);

/* GCC-PHAT lag (kept for comparison). */
double gcc_phat(const double *x, const double *y, int n, int max_lag);

/* Delay-and-sum beamformer: mics on ULA spacing d, angle deg from broadside. */
double beam_pattern_gain(const double *mic_re, const double *mic_im,
                         int nch, double spacing_m, double fs, double c,
                         double f, double look_deg, double src_deg);

#endif
