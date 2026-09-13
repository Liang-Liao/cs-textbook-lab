#include "blowfish.h"
#include "crypto_common.h"

/* Chapter 14: Blowfish educational implementation.
 * P-array uses the standard fractional-pi constants. S-boxes are
 * re-initialized from a reduced seed (not the full pi tables) so the
 * lab stays small — structure/key-schedule learning only, no official KAT. */

static const uint32_t P_INIT[18] = {
    0x243f6a88,0x85a308d3,0x13198a2e,0x03707344,0xa4093822,0x299f31d0,
    0x082efa98,0xec4e6c89,0x452821e6,0x38d01377,0xbe5466cf,0x34e90c6c,
    0xc0ac29b7,0xc97c50dd,0x3f84d5b5,0xb5470917,0x9216d5d9,0x8979fb1b
};

static uint32_t F(const blowfish_ctx_t *ctx, uint32_t x)
{
    uint8_t a = (uint8_t)(x >> 24);
    uint8_t b = (uint8_t)(x >> 16);
    uint8_t c = (uint8_t)(x >> 8);
    uint8_t d = (uint8_t)x;
    uint32_t y = ctx->S[0][a] + ctx->S[1][b];
    y ^= ctx->S[2][c];
    y += ctx->S[3][d];
    return y;
}

static void encrypt_block(const blowfish_ctx_t *ctx, uint32_t *xl, uint32_t *xr)
{
    uint32_t Xl = *xl, Xr = *xr;
    int i;
    for (i = 0; i < 16; i++) {
        Xl ^= ctx->P[i];
        Xr ^= F(ctx, Xl);
        { uint32_t t = Xl; Xl = Xr; Xr = t; }
    }
    { uint32_t t = Xl; Xl = Xr; Xr = t; }
    Xr ^= ctx->P[16];
    Xl ^= ctx->P[17];
    *xl = Xl;
    *xr = Xr;
}

static void decrypt_block(const blowfish_ctx_t *ctx, uint32_t *xl, uint32_t *xr)
{
    uint32_t Xl = *xl, Xr = *xr;
    int i;
    for (i = 17; i > 1; i--) {
        Xl ^= ctx->P[i];
        Xr ^= F(ctx, Xl);
        { uint32_t t = Xl; Xl = Xr; Xr = t; }
    }
    { uint32_t t = Xl; Xl = Xr; Xr = t; }
    Xr ^= ctx->P[1];
    Xl ^= ctx->P[0];
    *xl = Xl;
    *xr = Xr;
}

void blowfish_init(blowfish_ctx_t *ctx, const uint8_t *key, size_t keylen)
{
    int i, j;
    uint32_t datal = 0, datar = 0;

    if (!ctx) return;
    memcpy(ctx->P, P_INIT, sizeof(ctx->P));
    for (i = 0; i < 4; i++)
        for (j = 0; j < 256; j++)
            ctx->S[i][j] = P_INIT[(i * 256 + j) % 18] ^ (uint32_t)(j * 0x9E3779B9u);

    if (!key || keylen == 0) {
        /* refuse empty key: leave a known-zero (unusable) state, like rc4_init */
        memset(ctx->P, 0, sizeof(ctx->P));
        memset(ctx->S, 0, sizeof(ctx->S));
        return;
    }
    for (i = 0, j = 0; i < 18; i++) {
        uint32_t k = 0;
        int t;
        for (t = 0; t < 4; t++) {
            k = (k << 8) | key[j];
            j = (int)((j + 1) % keylen);
        }
        ctx->P[i] ^= k;
    }

    for (i = 0; i < 18; i += 2) {
        encrypt_block(ctx, &datal, &datar);
        ctx->P[i] = datal;
        ctx->P[i + 1] = datar;
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 256; j += 2) {
            encrypt_block(ctx, &datal, &datar);
            ctx->S[i][j] = datal;
            ctx->S[i][j + 1] = datar;
        }
    }
}

void blowfish_encrypt_block(const blowfish_ctx_t *ctx, const uint8_t in[8], uint8_t out[8])
{
    uint32_t xl, xr;
    xl = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
    xr = ((uint32_t)in[4] << 24) | ((uint32_t)in[5] << 16) | ((uint32_t)in[6] << 8) | in[7];
    encrypt_block(ctx, &xl, &xr);
    out[0] = (uint8_t)(xl >> 24); out[1] = (uint8_t)(xl >> 16);
    out[2] = (uint8_t)(xl >> 8);  out[3] = (uint8_t)xl;
    out[4] = (uint8_t)(xr >> 24); out[5] = (uint8_t)(xr >> 16);
    out[6] = (uint8_t)(xr >> 8);  out[7] = (uint8_t)xr;
}

void blowfish_decrypt_block(const blowfish_ctx_t *ctx, const uint8_t in[8], uint8_t out[8])
{
    uint32_t xl, xr;
    xl = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
    xr = ((uint32_t)in[4] << 24) | ((uint32_t)in[5] << 16) | ((uint32_t)in[6] << 8) | in[7];
    decrypt_block(ctx, &xl, &xr);
    out[0] = (uint8_t)(xl >> 24); out[1] = (uint8_t)(xl >> 16);
    out[2] = (uint8_t)(xl >> 8);  out[3] = (uint8_t)xl;
    out[4] = (uint8_t)(xr >> 24); out[5] = (uint8_t)(xr >> 16);
    out[6] = (uint8_t)(xr >> 8);  out[7] = (uint8_t)xr;
}

void blowfish_encrypt_raw(const uint8_t in[8], uint8_t out[8], const void *key)
{
    blowfish_encrypt_block((const blowfish_ctx_t *)key, in, out);
}

void blowfish_decrypt_raw(const uint8_t in[8], uint8_t out[8], const void *key)
{
    blowfish_decrypt_block((const blowfish_ctx_t *)key, in, out);
}
