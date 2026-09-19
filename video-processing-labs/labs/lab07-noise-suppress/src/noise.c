/* copied from lab04-random-wiener (adapted for lab07-noise-suppress) */
#include "noise.h"
#include <math.h>
static unsigned g_s = 1;
void rng_seed(unsigned s) { g_s = s ? s : 1; }
double rng_u(void) {
    unsigned x = g_s; x ^= x<<13; x ^= x>>17; x ^= x<<5; g_s = x;
    return ((x & 0xffffff) + 0.5) / 16777216.0;
}
void add_white(double *x, size_t n, double sigma, unsigned seed) {
    rng_seed(seed);
    for (size_t i = 0; i + 1 < n; i += 2) {
        double u1 = rng_u(), u2 = rng_u();
        double r = sigma * sqrt(-2.0 * log(u1));
        double th = 6.283185307179586 * u2;
        x[i] += r * cos(th);
        x[i+1] += r * sin(th);
    }
}
double sig_power(const double *x, size_t n) {
    double s = 0; for (size_t i = 0; i < n; i++) s += x[i]*x[i];
    return s / (double)n;
}
