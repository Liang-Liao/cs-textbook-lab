#include "secret_share.h"
#include "crypto_common.h"

/* Polynomial evaluation with random coefficients (degree k-1). */

int ss_split(uint64_t secret, int n, int k, uint64_t p,
             ss_share_t out[SS_MAX_SHARES])
{
    uint64_t coef[SS_MAX_SHARES];
    int i, j;

    if (n < 1 || k < 1 || k > n || n > SS_MAX_SHARES) return -1;
    if (p < 3 || secret >= p) return -1;

    coef[0] = secret % p;
    for (i = 1; i < k; i++) {
        uint8_t rnd[8];
        uint64_t r;
        if (crypto_random_bytes(rnd, 8) != 0) return -1;
        memcpy(&r, rnd, 8);
        coef[i] = r % p;
    }

    for (i = 0; i < n; i++) {
        uint64_t x = (uint64_t)(i + 1);
        uint64_t y = 0;
        /* Horner: y = c0 + c1*x + ... + c_{k-1}*x^{k-1}  (mod p) */
        for (j = k - 1; j >= 0; j--) {
            y = mod_mul_u64(y, x, p);
            y += coef[j];
            if (y >= p) y -= p;
        }
        out[i].x = x;
        out[i].y = y;
    }
    return 0;
}

int ss_recover(const ss_share_t *shares, int count, int k, uint64_t p,
               uint64_t *secret_out)
{
    uint64_t s = 0;
    int i, j;

    if (count < k || k < 1 || p < 3) return -1;

    /* Lagrange at 0: s = sum_i y_i * prod_{j!=i} (0-x_j)/(x_i-x_j) */
    for (i = 0; i < k; i++) {
        uint64_t num = 1, den = 1, term;
        for (j = 0; j < k; j++) {
            if (i == j) continue;
            num = mod_mul_u64(num, (p - shares[j].x) % p, p);
            {
                uint64_t d = (shares[i].x + p - shares[j].x) % p;
                uint64_t dinv = 0;
                if (d == 0) return -1;
                if (mod_inv_u64(d, p, &dinv) != 0) return -1;
                den = mod_mul_u64(den, dinv, p);
            }
        }
        term = mod_mul_u64(shares[i].y, num, p);
        term = mod_mul_u64(term, den, p);
        s += term;
        if (s >= p) s -= p;
    }
    *secret_out = s;
    return 0;
}
