#include "rsa.h"
#include "crypto_common.h"

int rsa_generate(rsa_keypair_t *kp, uint64_t p_lo, uint64_t p_hi, uint64_t e)
{
    uint64_t p, q, n, phi, d;
    int tries;

    if (e < 3 || (e & 1) == 0) e = 65537;
    for (tries = 0; tries < 200; tries++) {
        if (random_prime_u64(p_lo, p_hi, 8, &p) != 0) return -1;
        if (random_prime_u64(p_lo, p_hi, 8, &q) != 0) return -1;
        if (p == q) continue;
        if (p != 0 && q > UINT64_MAX / p) continue; /* n would wrap */
        n = p * q;
        if (p != 1 && (q - 1) > UINT64_MAX / (p - 1)) continue;
        phi = (p - 1) * (q - 1);
        if (gcd_u64(e, phi) != 1) {
            /* try e=3 for toy keys */
            uint64_t e2 = 3;
            if (gcd_u64(e2, phi) != 1) continue;
            e = e2;
        }
        if (mod_inv_u64(e, phi, &d) != 0) continue;
        kp->n = n;
        kp->e = e;
        kp->d = d;
        return 0;
    }
    return -1;
}

uint64_t rsa_public(uint64_t m, const rsa_keypair_t *kp)
{
    return mod_pow_u64(m, kp->e, kp->n);
}

uint64_t rsa_private(uint64_t c, const rsa_keypair_t *kp)
{
    return mod_pow_u64(c, kp->d, kp->n);
}

static uint64_t bytes_to_int(const uint8_t *b, size_t n)
{
    uint64_t v = 0;
    size_t i;
    for (i = 0; i < n; i++)
        v = (v << 8) | b[i];
    return v;
}

static size_t int_to_bytes(uint64_t v, uint8_t *out, size_t max)
{
    uint8_t tmp[8];
    int i, start;
    for (i = 0; i < 8; i++)
        tmp[i] = (uint8_t)(v >> (8 * (7 - i)));
    start = 0;
    while (start < 8 && tmp[start] == 0) start++;
    {
        size_t n = (size_t)(8 - start);
        if (n == 0) { out[0] = 0; return 1; }
        if (n > max) n = max;
        memcpy(out, tmp + start, n);
        return n;
    }
}

static size_t n_bytes(uint64_t n)
{
    size_t k = 0;
    if (n == 0) return 1;
    while (n) { n >>= 8; k++; }
    return k;
}

/* Educational "padding": 0x02 || PS || 0x00 || M  (no leading 00 of PKCS#1)
 * length = 1 + need + 1 + msg_len = k  =>  need = k - 2 - msg_len
 * Requires need >= 1 so the 0x00 separator is unambiguous. */
int rsa_public_bytes(const uint8_t *msg, size_t msg_len,
                     uint8_t *out, size_t *out_len,
                     const rsa_keypair_t *kp)
{
    size_t k = n_bytes(kp->n);
    uint8_t em[8];
    uint64_t m, c;
    size_t need, i;

    if (k < 4 || msg_len == 0 || msg_len + 3 > k)
        return -1;
    need = k - 2 - msg_len;
    if (need < 1) return -1;
    memset(em, 0, k);
    em[0] = 0x02;
    for (i = 0; i < need; i++) {
        uint8_t r = 1;
        crypto_random_bytes(&r, 1);
        if (r == 0) r = 1;
        em[1 + i] = r;
    }
    em[1 + need] = 0x00;
    memcpy(em + 2 + need, msg, msg_len);

    m = bytes_to_int(em, k);
    if (m >= kp->n) return -1;
    c = rsa_public(m, kp);
    {
        uint8_t tmp[8];
        size_t n = int_to_bytes(c, tmp, 8);
        memset(out, 0, k);
        if (n > k) return -1;
        memcpy(out + (k - n), tmp, n);
        *out_len = k;
    }
    return 0;
}

int rsa_private_bytes(const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t *out_len,
                      const rsa_keypair_t *kp)
{
    size_t k = n_bytes(kp->n);
    uint8_t em[8];
    uint64_t c, m;
    size_t i;

    if (in_len != k || k > 8) return -1;
    c = bytes_to_int(in, in_len);
    m = rsa_private(c, kp);
    /* write m as exactly k big-endian bytes (m < n) */
    for (i = 0; i < k; i++)
        em[k - 1 - i] = (uint8_t)(m >> (8 * i));
    if (em[0] != 0x02) return -1;
    for (i = 1; i < k; i++) {
        if (em[i] == 0x00) {
            size_t mlen = k - i - 1;
            if (mlen == 0) return -1;
            memcpy(out, em + i + 1, mlen);
            *out_len = mlen;
            return 0;
        }
    }
    return -1;
}
