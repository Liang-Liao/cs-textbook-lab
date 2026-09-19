#ifndef LAB06_LPC_H
#define LAB06_LPC_H

#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Autocorrelation r[0..p] of frame x[0..n-1]. */
void autocorr(const double *x, int n, int p, double *r);

/* Levinson-Durbin: A(z)=1+a1 z^{-1}+...+ap z^{-p}. *err = residual energy. */
int levinson_durbin(const double *r, int p, double *a, double *err);

/* LPC envelope |1/A(e^{jw})| sampled at nfft bins (0..nfft-1, real signal → use nfft). */
void lpc_envelope(const double *a, int p, int nfft, double *env_db);

/* Peak-pick formants from LPC envelope (Hz). Returns count. */
int formants_from_envelope(const double *env_db, int nfft, double fs, int max_formants,
                           double *f_hz);

/* Build all-pole A(z) from formant table (cascade of resonators). */
void lpc_from_formants(const double *f, const double *bw, int n_f, double fs, double *a,
                       int p);

/* Synthesize voiced frame: impulse train through 1/A(z). */
void lpc_synthesize_vowel(const double *a, int p, int n, double fs, double f0, double *x);

#endif
