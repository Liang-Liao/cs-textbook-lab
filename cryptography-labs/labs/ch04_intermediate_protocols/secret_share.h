#ifndef SECRET_SHARE_H
#define SECRET_SHARE_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 4: Shamir's secret sharing over GF(p), educational uint64.
 * Secret s in [0, p). Shares are (x, y) with x = 1..n, y = f(x) mod p.
 * Lagrange interpolation recovers s = f(0) from any k shares. */

#define SS_MAX_SHARES 16

typedef struct {
    uint64_t x;
    uint64_t y;
} ss_share_t;

/* p must be prime > secret and > n. */
int ss_split(uint64_t secret, int n, int k, uint64_t p,
             ss_share_t out[SS_MAX_SHARES]);

int ss_recover(const ss_share_t *shares, int count, int k, uint64_t p,
               uint64_t *secret_out);

#endif
