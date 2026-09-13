#include "advproto.h"
#include "sha256.h"
#include "crypto_common.h"

/* Domain-separated commitment: c = H(rlen || r || mlen || m) */
void commit(const uint8_t *r, size_t rlen,
            const uint8_t *m, size_t mlen,
            uint8_t c[32])
{
    sha256_ctx_t ctx;
    uint8_t lenbuf[16];
    int i;
    for (i = 0; i < 8; i++) {
        lenbuf[i]     = (uint8_t)((uint64_t)rlen >> (8 * i));
        lenbuf[8 + i] = (uint8_t)((uint64_t)mlen >> (8 * i));
    }
    sha256_init(&ctx);
    sha256_update(&ctx, lenbuf, 16);
    sha256_update(&ctx, r, rlen);
    sha256_update(&ctx, m, mlen);
    sha256_final(&ctx, c);
}

int open_check(const uint8_t *r, size_t rlen,
               const uint8_t *m, size_t mlen,
               const uint8_t c[32])
{
    uint8_t t[32];
    commit(r, rlen, m, mlen, t);
    return memeq_const(t, c, 32);
}

static void fs_challenge(const zk_pub_t *pub, uint64_t A, uint64_t *c_out)
{
    uint8_t buf[32], ch[32];
    uint64_t i;
    for (i = 0; i < 8; i++) {
        buf[i]      = (uint8_t)(pub->g >> (8 * i));
        buf[8 + i]  = (uint8_t)(pub->X >> (8 * i));
        buf[16 + i] = (uint8_t)(A >> (8 * i));
        buf[24 + i] = (uint8_t)(pub->p >> (8 * i));
    }
    sha256(buf, 32, ch);
    memcpy(c_out, ch, 8);
    *c_out %= (pub->p - 1);
}

int zk_prove(const zk_pub_t *pub, uint64_t x, zk_proof_t *proof)
{
    uint64_t r = 0;
    uint8_t rnd[8];
    /* No silent fallback nonce: a fixed r would make the proof replayable. */
    if (proof == NULL || pub == NULL || crypto_random_bytes(rnd, 8) != 0)
        return -1;
    memcpy(&r, rnd, 8);
    r = 2 + (r % (pub->p - 3));
    proof->A = mod_pow_u64(pub->g, r, pub->p);
    fs_challenge(pub, proof->A, &proof->c);
    proof->z = (r + mod_mul_u64(proof->c, x, pub->p - 1)) % (pub->p - 1);
    return 0;
}

int zk_verify(const zk_pub_t *pub, const zk_proof_t *proof)
{
    uint64_t expect_c, left, right;
    if (proof->A == 0 || proof->A >= pub->p) return 0;
    if (proof->z >= pub->p - 1) return 0;
    /* Fiat-Shamir: recompute c from transcript, do not trust proof->c blindly */
    fs_challenge(pub, proof->A, &expect_c);
    if (expect_c != proof->c) return 0;
    left  = mod_pow_u64(pub->g, proof->z, pub->p);
    right = mod_mul_u64(proof->A, mod_pow_u64(pub->X, proof->c, pub->p), pub->p);
    return left == right;
}
