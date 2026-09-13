#include "sha1.h"
#include "crypto_common.h"

static uint32_t rotl32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void sha1_transform(uint32_t h[5], const uint8_t block[64])
{
    uint32_t w[80], a, b, c, d, e, f, k, temp;
    int i;

    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    for (i = 16; i < 80; i++)
        w[i] = rotl32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);

    a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
    for (i = 0; i < 80; i++) {
        if (i < 20)      { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d;           k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else             { f = b ^ c ^ d;           k = 0xCA62C1D6; }
        temp = rotl32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rotl32(b, 30); b = a; a = temp;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void sha1_init(sha1_ctx_t *ctx)
{
    ctx->h[0] = 0x67452301;
    ctx->h[1] = 0xEFCDAB89;
    ctx->h[2] = 0x98BADCFE;
    ctx->h[3] = 0x10325476;
    ctx->h[4] = 0xC3D2E1F0;
    ctx->bitcount = 0;
    memset(ctx->buffer, 0, 64);
}

void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t idx = (size_t)((ctx->bitcount / 8) % 64);
    ctx->bitcount += (uint64_t)len * 8;
    while (len > 0) {
        size_t space = 64 - idx;
        size_t take = (len < space) ? len : space;
        memcpy(ctx->buffer + idx, data, take);
        idx += take; data += take; len -= take;
        if (idx == 64) {
            sha1_transform(ctx->h, ctx->buffer);
            idx = 0;
        }
    }
}

void sha1_final(sha1_ctx_t *ctx, uint8_t digest[20])
{
    uint8_t bits[8], pad = 0x80, zero = 0;
    int i;
    for (i = 0; i < 8; i++)
        bits[i] = (uint8_t)(ctx->bitcount >> (8 * (7 - i))); /* big-endian length */
    sha1_update(ctx, &pad, 1);
    while ((size_t)((ctx->bitcount / 8) % 64) != 56)
        sha1_update(ctx, &zero, 1);
    sha1_update(ctx, bits, 8);
    for (i = 0; i < 5; i++) {
        digest[i*4]   = (uint8_t)(ctx->h[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->h[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->h[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->h[i]);
    }
}

void sha1(const uint8_t *data, size_t len, uint8_t digest[20])
{
    sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(&ctx, digest);
}
