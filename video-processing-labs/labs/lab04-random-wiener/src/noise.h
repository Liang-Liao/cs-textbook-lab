#ifndef LAB04_NOISE_H
#define LAB04_NOISE_H
#include <stddef.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
void rng_seed(unsigned s);
double rng_uniform(void); /* (0,1) */
void rng_gauss(double *out, size_t n, double mu, double sigma);
void ar1(double *out, size_t n, double a1, double sigma_e, unsigned seed);
/* Add white noise so that SNR_db = 10log10(Ps/Pn). Returns noise power used. */
double add_noise_at_snr(double *sig, size_t n, double snr_db, unsigned seed);
double mean(const double *x, size_t n);
double var(const double *x, size_t n, double mu);
#endif
