#ifndef LAB03_FILTERS_H
#define LAB03_FILTERS_H

/* Windowed-sinc FIR lowpass. taps odd preferred for integer group delay. */
void fir_design_lowpass(double *h, int n_taps, double fs, double fc);

/* Apply FIR, y length = n_x + n_taps - 1 (full conv) or same-length if y_len==n_x (same). */
void fir_apply(const double *h, int n_taps, const double *x, int n, double *y);

/* RBJ biquad coefficients */
typedef struct {
    double b0, b1, b2, a1, a2; /* a0 normalized to 1 */
} biquad;

void biquad_lowpass(biquad *b, double fs, double f0, double q);
void biquad_peaking(biquad *b, double fs, double f0, double q, double gain_db);
void biquad_highshelf(biquad *b, double fs, double f0, double q, double gain_db);

/* Direct-form I state */
typedef struct {
    biquad c;
    double x1, x2, y1, y2;
} biquad_state;

void biquad_reset(biquad_state *s, biquad c);
double biquad_step(biquad_state *s, double x);
void biquad_process(const biquad *c, const double *x, int n, double *y);

/* Magnitude response |H(e^{jw})| at frequency f. */
double biquad_mag(const biquad *b, double fs, double f);
double fir_mag(const double *h, int n_taps, double fs, double f);

/* Find -3dB cutoff of lowpass biquad by frequency scan. */
double biquad_find_cutoff(const biquad *b, double fs, double f0_guess);

/* Linear-phase check: max |h[i]-h[n-1-i]| (type-I symmetry). */
double fir_impulse_asymmetry(const double *h, int n_taps);

/* Group delay spread via numerical d(phase)/dw across passband (samples). */
double fir_group_delay_spread(const double *h, int n_taps, double fs, double f_lo, double f_hi);

#endif
