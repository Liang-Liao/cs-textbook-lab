#ifndef CLRS_GROWTH_H
#define CLRS_GROWTH_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.3 Growth of Functions — numeric helpers.
 * Values use long double so large n can be compared before overflow.
 */

/* lg n (base 2). lg(0) and lg(negative) are undefined → return 0. */
long double growth_lg(long double n);

/* n * lg n */
long double growth_n_lg_n(long double n);

/* n^k for integer k >= 0 */
long double growth_n_pow(long double n, int k);

/* c^n (c > 0). Uses powl; for c=2 and small integer n can also use bit tricks. */
long double growth_exp(long double base, long double n);

/* lg(n!) via Stirling-ish sum for moderate n; exact product for small n. */
long double growth_lg_factorial(long double n);

/* Sum 1/i for i=1..n (harmonic number H_n) — appears in average-case analysis. */
long double growth_harmonic(long double n);

/*
 * Asymptotic classification helpers (educational, not rigorous proofs).
 * Compare f(n)/g(n) over n in [n0, n1] and report the trend of the ratio.
 */
typedef enum {
  GROWTH_RATIO_FASTER = 0, /* ratio → 0: f = o(g) */
  GROWTH_RATIO_SAME_ORDER, /* ratio → constant > 0: Θ relation plausible */
  GROWTH_RATIO_SLOWER,     /* ratio → ∞: g = o(f) */
  GROWTH_RATIO_UNCLEAR
} GrowthCompare;

/*
 * f_fn / g_fn take n and return the function value (must be > 0 for n >= n0).
 * Samples a few points from n0..n1 and classifies by the ratio trend.
 */
GrowthCompare growth_compare(
    long double (*f_fn)(long double n),
    long double (*g_fn)(long double n),
    long double n0, long double n1);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_GROWTH_H */
