#include "rng.h"
#include "dist.h"
#include "stats.h"
#include "gof.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MT_N 624
#define MT_M 397
#define MT_MATRIX_A 0x9908b0dfu
#define MT_UPPER 0x80000000u
#define MT_LOWER 0x7fffffffu
/* Knuth MMIX LCG, modulus 2^64 */
#define LCG_A 6364136223846793005ull
#define LCG_C 1442695040888963407ull
#define XORS_A 0x2545F4914F6CDD1Dull
#define SM_G 0x9E3779B97F4A7C15ull

const char *mlab_rng_kind_name(int kind)
{
    switch (kind) {
    case MLAB_RNG_SPLITMIX64: return "splitmix64";
    case MLAB_RNG_XORSHIFT64STAR: return "xorshift64*";
    case MLAB_RNG_LCG: return "lcg-mmix";
    case MLAB_RNG_MT19937: return "mt19937";
    default: return "unknown";
    }
}

static unsigned long long splitmix64_raw(unsigned long long *state)
{
    unsigned long long x = (*state += SM_G);
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

static unsigned long long xorshift64star_next(unsigned long long *state)
{
    unsigned long long x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * XORS_A;
}

static void mt_twist(mlab_rng *r)
{
    /* Standard MT19937: must not overwrite future keys before they are read. */
    int i;
    for (i = 0; i < MT_N - MT_M; ++i) {
        unsigned int y = (r->mt[i] & MT_UPPER) | (r->mt[i + 1] & MT_LOWER);
        r->mt[i] = r->mt[i + MT_M] ^ (y >> 1) ^ ((y & 1u) ? MT_MATRIX_A : 0u);
    }
    for (; i < MT_N - 1; ++i) {
        unsigned int y = (r->mt[i] & MT_UPPER) | (r->mt[i + 1] & MT_LOWER);
        r->mt[i] = r->mt[i + (MT_M - MT_N)] ^ (y >> 1) ^ ((y & 1u) ? MT_MATRIX_A : 0u);
    }
    {
        unsigned int y = (r->mt[MT_N - 1] & MT_UPPER) | (r->mt[0] & MT_LOWER);
        r->mt[MT_N - 1] = r->mt[MT_M - 1] ^ (y >> 1) ^ ((y & 1u) ? MT_MATRIX_A : 0u);
    }
    r->mt_idx = 0;
}

static unsigned int mt_next_u32(mlab_rng *r)
{
    unsigned int y;
    if (r->mt_idx >= MT_N) mt_twist(r);
    y = r->mt[r->mt_idx++];
    y ^= y >> 11;
    y ^= (y << 7) & 0x9d2c5680u;
    y ^= (y << 15) & 0xefc60000u;
    y ^= y >> 18;
    return y;
}

void mlab_rng_seed_kind(mlab_rng *r, unsigned long long seed, int kind)
{
    int i;
    if (!r) return;
    if (kind < MLAB_RNG_SPLITMIX64 || kind > MLAB_RNG_MT19937)
        kind = MLAB_RNG_SPLITMIX64;
    memset(r, 0, sizeof *r);
    r->kind = kind;
    r->s = seed ? seed : SM_G;
    r->lcg_s = r->s;
    /* fill MT table via splitmix so all 624 words are mixed */
    {
        unsigned long long sm = r->s;
        for (i = 0; i < MT_N; ++i) {
            unsigned long long z = splitmix64_raw(&sm);
            r->mt[i] = (unsigned int)(z ^ (z >> 32));
        }
        r->mt_idx = MT_N;
    }
    /* xorshift must be non-zero */
    if (r->s == 0) r->s = 0x9E3779B97F4A7C15ull;
}

void mlab_rng_seed(mlab_rng *r, unsigned long long seed)
{
    mlab_rng_seed_kind(r, seed, MLAB_RNG_SPLITMIX64);
}

unsigned long long mlab_rng_u64(mlab_rng *r)
{
    if (!r) return 0ull;
    switch (r->kind) {
    case MLAB_RNG_XORSHIFT64STAR:
        return xorshift64star_next(&r->s);
    case MLAB_RNG_LCG:
        r->lcg_s = r->lcg_s * LCG_A + LCG_C;
        return r->lcg_s;
    case MLAB_RNG_MT19937: {
        unsigned int a = mt_next_u32(r);
        unsigned int b = mt_next_u32(r);
        return ((unsigned long long)a << 32) | (unsigned long long)b;
    }
    case MLAB_RNG_SPLITMIX64:
    default:
        return splitmix64_raw(&r->s);
    }
}

double mlab_rng_uniform(mlab_rng *r)
{
    unsigned long long u;
    double x;
    if (!r) return 0.5;
    if (r->kind == MLAB_RNG_MT19937) {
        /* 53-bit from two draws */
        unsigned long long a = mt_next_u32(r) >> 5;   /* 27 bits */
        unsigned long long b = mt_next_u32(r) >> 6;   /* 26 bits */
        u = (a << 26) | b;
    } else {
        u = mlab_rng_u64(r) >> 11;
    }
    x = (double)u * (1.0 / 9007199254740992.0); /* 2^53 */
    if (x <= 0.0) x = 4.9406564584124654e-324;
    if (x >= 1.0) x = 1.0 - 2.220446049250313e-16;
    return x;
}

double mlab_rng_normal(mlab_rng *r)
{
    double u1, u2, z;
    if (!r) return 0.0;
    if (r->normal_spare_valid) {
        r->normal_spare_valid = 0;
        return r->normal_spare;
    }
    u1 = mlab_rng_uniform(r);
    u2 = mlab_rng_uniform(r);
    z = sqrt(-2.0 * log(u1));
    r->normal_spare = z * sin(6.283185307179586476 * u2);
    r->normal_spare_valid = 1;
    return z * cos(6.283185307179586476 * u2);
}

double mlab_rng_normal_polar(mlab_rng *r)
{
    double u, v, s, f;
    if (!r) return 0.0;
    if (r->normal_spare_valid) {
        r->normal_spare_valid = 0;
        return r->normal_spare;
    }
    for (;;) {
        u = 2.0 * mlab_rng_uniform(r) - 1.0;
        v = 2.0 * mlab_rng_uniform(r) - 1.0;
        s = u * u + v * v;
        if (s > 0.0 && s < 1.0) break;
    }
    f = sqrt(-2.0 * log(s) / s);
    r->normal_spare = v * f;
    r->normal_spare_valid = 1;
    return u * f;
}

double mlab_rng_normal_inv(mlab_rng *r)
{
    double u;
    if (!r) return 0.0;
    u = mlab_rng_uniform(r);
    return mlab_normal_quantile(u, 0.0, 1.0);
}

double mlab_rng_exponential(mlab_rng *r, double lambda)
{
    double u;
    if (!r || lambda <= 0.0) return 0.0;
    u = mlab_rng_uniform(r);
    return -log(u) / lambda;
}

int mlab_rng_bernoulli(mlab_rng *r, double p)
{
    return mlab_rng_uniform(r) < p ? 1 : 0;
}

int mlab_rng_poisson(mlab_rng *r, double lambda)
{
    if (lambda < 30.0) {
        double L = exp(-lambda);
        int k = 0;
        double p = 1.0;
        do {
            ++k;
            p *= mlab_rng_uniform(r);
        } while (p > L);
        return k - 1;
    } else {
        int k = (int)floor(lambda + sqrt(lambda) * mlab_rng_normal(r) + 0.5);
        return k < 0 ? 0 : k;
    }
}

double mlab_rng_chi2_uniform(mlab_rng *r, int n, int k, double *pval)
{
    long *obs;
    double *expn;
    double e, chi2;
    int i, b;

    if (!r || n <= 0 || k <= 1) return 0.0;
    obs = (long *)calloc((size_t)k, sizeof(long));
    expn = (double *)malloc((size_t)k * sizeof(double));
    if (!obs || !expn) {
        free(obs);
        free(expn);
        return 0.0;
    }
    e = (double)n / (double)k;
    for (i = 0; i < k; ++i) expn[i] = e;
    for (i = 0; i < n; ++i) {
        double u = mlab_rng_uniform(r);
        b = (int)(u * (double)k);
        if (b < 0) b = 0;
        if (b >= k) b = k - 1;
        obs[b] += 1;
    }
    chi2 = mlab_chi2_gof(obs, expn, k);
    if (pval) *pval = mlab_chi2_sf(chi2, k - 1);
    free(obs);
    free(expn);
    return chi2;
}

double mlab_autocorr(const double *x, int n, int lag)
{
    double mean, var = 0.0, acc = 0.0;
    int i;
    if (!x || n <= lag + 1 || lag < 1) return 0.0;
    mean = mlab_mean(x, n);
    for (i = 0; i < n; ++i) {
        double d = x[i] - mean;
        var += d * d;
    }
    if (var <= 0.0) return 0.0;
    for (i = 0; i + lag < n; ++i)
        acc += (x[i] - mean) * (x[i + lag] - mean);
    return acc / var;
}

double mlab_rng_max_abs_acf(mlab_rng *r, int n, int max_lag, int *argmax_lag)
{
    double *x;
    double best = 0.0;
    int lag, best_lag = 0;
    if (!r || n <= 0 || max_lag < 1) return 0.0;
    x = (double *)malloc((size_t)n * sizeof(double));
    if (!x) return 0.0;
    for (lag = 0; lag < n; ++lag) x[lag] = mlab_rng_uniform(r);
    for (lag = 1; lag <= max_lag && lag < n; ++lag) {
        double a = fabs(mlab_autocorr(x, n, lag));
        if (a > best) {
            best = a;
            best_lag = lag;
        }
    }
    free(x);
    if (argmax_lag) *argmax_lag = best_lag;
    return best;
}
