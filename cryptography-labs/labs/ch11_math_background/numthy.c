#include "numthy.h"
#include "crypto_common.h"

uint64_t euler_phi_u64(uint64_t n)
{
    uint64_t result = n;
    uint64_t p;
    if (n == 0) return 0;
    /* p <= n/p (not p*p <= n): p*p wraps for n near 2^64 and would loop/hang */
    for (p = 2; p <= n / p; p++) {
        if (n % p == 0) {
            while (n % p == 0) n /= p;
            result -= result / p;
        }
    }
    if (n > 1) result -= result / n;
    return result;
}

int crt_u64(uint64_t a1, uint64_t m1, uint64_t a2, uint64_t m2, uint64_t *x)
{
    uint64_t inv = 0, t;
    if (gcd_u64(m1, m2) != 1) return -1;
    if (mod_inv_u64(m1, m2, &inv) != 0) return -1;
    t = mod_mul_u64((a2 + m2 - a1 % m2) % m2, inv, m2);
    *x = a1 + m1 * t;
    return 0;
}
