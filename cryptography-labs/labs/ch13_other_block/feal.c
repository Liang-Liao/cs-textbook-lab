#include "feal.h"
#include "crypto_common.h"

/* FEAL S-boxes (public FEAL specification). */
static uint8_t S0(uint8_t a, uint8_t b)
{
    uint8_t x = (uint8_t)(a + b);
    x = (uint8_t)((x << 2) | (x >> 6)); /* rotl 2 */
    return x;
}

static uint8_t S1(uint8_t a, uint8_t b)
{
    uint8_t x = (uint8_t)(a + b + 1);
    x = (uint8_t)((x << 2) | (x >> 6));
    return x;
}

/* FEAL-style round function (structure teaching; not the official FEAL f).
 * All four bytes of both inputs feed the S0/S1 cascade, like real FEAL. */
static uint32_t f(uint32_t a, uint32_t b)
{
    uint8_t a1 = (uint8_t)(a >> 24), a2 = (uint8_t)(a >> 16),
            a3 = (uint8_t)(a >> 8),  a4 = (uint8_t)a;
    uint8_t b1 = (uint8_t)(b >> 24), b2 = (uint8_t)(b >> 16),
            b3 = (uint8_t)(b >> 8),  b4 = (uint8_t)b;
    uint8_t x1, x2, x3, x4;
    x1 = S1((uint8_t)(a1 ^ b1), (uint8_t)(a2 ^ b2));
    x2 = S0((uint8_t)(a2 ^ b2), (uint8_t)(a3 ^ b3));
    x3 = S0((uint8_t)(a3 ^ b3), (uint8_t)(a4 ^ b4));
    x4 = S1((uint8_t)(a4 ^ b4), x1);
    return ((uint32_t)(x1 ^ a1) << 24) | ((uint32_t)(x2 ^ a2) << 16) |
           ((uint32_t)(x3 ^ a3) << 8) | (uint32_t)(x4 ^ a4);
}

static uint32_t load_be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void store_be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void feal8_setkey(feal_ctx_t *ctx, const uint8_t key[8])
{
    /* Simplified key schedule: expand 64-bit key into 8 round keys. */
    uint32_t A = load_be(key);
    uint32_t B = load_be(key + 4);
    int i;
    for (i = 0; i < 8; i++) {
        ctx->K[i] = A ^ (uint32_t)(i * 0x9E3779B9u);
        A = f(A, B);
        B = f(B, A ^ 0xA5A5A5A5u);
    }
}

void feal8_encrypt(const feal_ctx_t *ctx, const uint8_t in[8], uint8_t out[8])
{
    uint32_t L = load_be(in), R = load_be(in + 4);
    int i;
    L ^= ctx->K[0];
    R ^= ctx->K[1];
    for (i = 0; i < 8; i++) {
        uint32_t t = L;
        L = R;
        R = t ^ f(R, ctx->K[i]);
    }
    store_be(out, R);
    store_be(out + 4, L);
}

void feal8_decrypt(const feal_ctx_t *ctx, const uint8_t in[8], uint8_t out[8])
{
    uint32_t R = load_be(in), L = load_be(in + 4);
    int i;
    for (i = 7; i >= 0; i--) {
        uint32_t t = R;
        R = L;
        L = t ^ f(L, ctx->K[i]);
    }
    /* undo last swap convention: after loop L,R swapped relative to encrypt */
    R ^= ctx->K[1];
    L ^= ctx->K[0];
    store_be(out, L);
    store_be(out + 4, R);
}
