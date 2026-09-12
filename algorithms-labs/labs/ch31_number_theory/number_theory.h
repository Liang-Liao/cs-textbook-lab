#ifndef CLRS_NUMBER_THEORY_H
#define CLRS_NUMBER_THEORY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.31 Number-theoretic algorithms.
 * Work in int64_t; caller keeps values in range to avoid overflow
 * for modular ops (a,b < m).
 */

/* CLRS 31.6 MODULAR-EXPONENTIATION: a^b mod n, b >= 0, n > 0. */
int64_t mod_exp(int64_t a, int64_t b, int64_t n);

/* CLRS 31.2 EUCLID(a,b): gcd, non-negative inputs. */
int64_t gcd_int(int64_t a, int64_t b);

/*
 * CLRS 31.3 EXTENDED-EUCLID: computes x,y,d with ax+by=d=gcd(a,b).
 * Returns d. Writes x,y.
 */
int64_t extended_euclid(int64_t a, int64_t b, int64_t *x, int64_t *y);

/*
 * Modular inverse of a mod m, requires gcd(a,m)=1.
 * Returns inverse in [0,m), or -1 if not invertible.
 */
int64_t modular_inverse(int64_t a, int64_t m);

/* Chinese remainder: x ≡ a (mod n), x ≡ b (mod m), n,m > 0.
 * Requires gcd(n,m)=1. Returns unique x in [0, n*m), or -1 on failure. */
int64_t crt_pair(int64_t a, int64_t n, int64_t b, int64_t m);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_NUMBER_THEORY_H */
