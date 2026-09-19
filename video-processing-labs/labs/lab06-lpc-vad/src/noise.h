/* copied from lab04-random-wiener */
#ifndef LAB06_NOISE_H
#define LAB06_NOISE_H
#include <stddef.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
void rng_seed(unsigned s);
double rng_uniform(void);
void rng_gauss(double *out, size_t n, double mu, double sigma);
double add_noise_at_snr(double *sig, size_t n, double snr_db, unsigned seed);
double mean(const double *x, size_t n);
double var(const double *x, size_t n, double mu);
#endif
