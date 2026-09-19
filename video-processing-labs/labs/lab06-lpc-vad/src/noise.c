/* copied from lab04-random-wiener (adapted for lab06-lpc-vad) */
#include "noise.h"
#include <math.h>
#include <stdlib.h>

static unsigned g_state = 1u;

void rng_seed(unsigned s) { g_state = s ? s : 1u; }

double rng_uniform(void) {
    unsigned x = g_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_state = x;
    return ((double)(x & 0xffffff) + 0.5) / 16777216.0;
}

void rng_gauss(double *out, size_t n, double mu, double sigma) {
    for (size_t i = 0; i + 1 < n; i += 2) {
        double u1 = rng_uniform(), u2 = rng_uniform();
        double r = sqrt(-2.0 * log(u1));
        double th = 2.0 * M_PI * u2;
        out[i] = mu + sigma * r * cos(th);
        out[i + 1] = mu + sigma * r * sin(th);
    }
    if (n & 1) {
        double u1 = rng_uniform(), u2 = rng_uniform();
        out[n - 1] = mu + sigma * sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    }
}

double add_noise_at_snr(double *sig, size_t n, double snr_db, unsigned seed) {
    double ps = 0.0;
    for (size_t i = 0; i < n; i++) ps += sig[i] * sig[i];
    ps /= (double)n;
    double pn = ps / pow(10.0, snr_db / 10.0);
    double sigma = sqrt(pn);
    rng_seed(seed);
    for (size_t i = 0; i < n; i += 2) {
        double u1 = rng_uniform(), u2 = rng_uniform();
        double r = sigma * sqrt(-2.0 * log(u1));
        double th = 2.0 * M_PI * u2;
        sig[i] += r * cos(th);
        if (i + 1 < n) sig[i + 1] += r * sin(th);
    }
    return pn;
}

double mean(const double *x, size_t n) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) s += x[i];
    return s / (double)n;
}

double var(const double *x, size_t n, double mu) {
    double s = 0.0;
    for (size_t i = 0; i < n; i++) {
        double d = x[i] - mu;
        s += d * d;
    }
    return s / (double)n;
}
