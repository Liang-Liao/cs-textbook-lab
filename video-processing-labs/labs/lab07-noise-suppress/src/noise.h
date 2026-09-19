/* copied from lab04-random-wiener */
#ifndef LAB07_NOISE_H
#define LAB07_NOISE_H
#include <stddef.h>
void rng_seed(unsigned s);
double rng_u(void);
void add_white(double *x, size_t n, double sigma, unsigned seed);
double sig_power(const double *x, size_t n);
#endif
