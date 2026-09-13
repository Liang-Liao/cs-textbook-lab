#include "oneway.h"
#include "crypto_common.h"

/* Toy 64-bit block function used as the compression core (Davies-Meyer).
 * Constants chosen for diffusion only. */
static uint64_t toy_block(uint64_t block, uint64_t key)
{
    int i;
    uint64_t x = block ^ key;
    for (i = 0; i < 8; i++) {
        x = (x << 13) | (x >> 51);
        x *= 0x9E3779B97F4A7C15ULL;
        x ^= (x >> 29);
        x += 0xD1B54A32D192ED03ULL;
        x ^= key;
    }
    return x;
}

/* Davies–Meyer: h' = E_m(h) XOR h  (key = message block, input = chaining) */
static void dm_compress(uint64_t h[4], const uint8_t block[32])
{
    uint64_t m0, m1, m2, m3;
    memcpy(&m0, block + 0, 8);
    memcpy(&m1, block + 8, 8);
    memcpy(&m2, block + 16, 8);
    memcpy(&m3, block + 24, 8);

    h[0] = toy_block(h[0], m0) ^ h[0];
    h[1] = toy_block(h[1] ^ m0, m1) ^ h[1];
    h[2] = toy_block(h[2] ^ m1, m2) ^ h[2];
    h[3] = toy_block(h[3] ^ m2, m3) ^ h[3];

    /* mild mixing between lanes */
    h[0] ^= h[3];
    h[1] ^= h[0];
    h[2] ^= h[1];
    h[3] ^= h[2];
}

void oneway_init(oneway_ctx_t *ctx)
{
    ctx->h[0] = 0x6A09E667F3BCC908ULL;
    ctx->h[1] = 0xBB67AE8584CAA73BULL;
    ctx->h[2] = 0x3C6EF372FE94F82BULL;
    ctx->h[3] = 0xA54FF53A5F1D36F1ULL;
    ctx->nbytes = 0;
    ctx->buflen = 0;
}

void oneway_update(oneway_ctx_t *ctx, const uint8_t *data, size_t len)
{
    ctx->nbytes += len;
    while (len > 0) {
        size_t take = 32 - ctx->buflen;
        if (take > len) take = len;
        memcpy(ctx->buf + ctx->buflen, data, take);
        ctx->buflen += take;
        data += take;
        len -= take;
        if (ctx->buflen == 32) {
            dm_compress(ctx->h, ctx->buf);
            ctx->buflen = 0;
        }
    }
}

void oneway_final(oneway_ctx_t *ctx, uint8_t out[32])
{
    uint64_t bitlen = ctx->nbytes * 8;
    uint8_t pad[72];
    size_t i;
    size_t used = ctx->buflen + 1 + 8;
    size_t rem = used % 32;
    size_t zeros = rem ? (32 - rem) : 0;
    size_t padlen = 1 + zeros + 8;

    memset(pad, 0, sizeof(pad));
    pad[0] = 0x80;
    for (i = 0; i < 8; i++)
        pad[1 + zeros + i] = (uint8_t)(bitlen >> (8 * i));

    {
        oneway_ctx_t t = *ctx;
        oneway_update(&t, pad, padlen);
        ctx->h[0] = t.h[0];
        ctx->h[1] = t.h[1];
        ctx->h[2] = t.h[2];
        ctx->h[3] = t.h[3];
    }

    for (i = 0; i < 4; i++) {
        out[i * 8 + 0] = (uint8_t)(ctx->h[i]);
        out[i * 8 + 1] = (uint8_t)(ctx->h[i] >> 8);
        out[i * 8 + 2] = (uint8_t)(ctx->h[i] >> 16);
        out[i * 8 + 3] = (uint8_t)(ctx->h[i] >> 24);
        out[i * 8 + 4] = (uint8_t)(ctx->h[i] >> 32);
        out[i * 8 + 5] = (uint8_t)(ctx->h[i] >> 40);
        out[i * 8 + 6] = (uint8_t)(ctx->h[i] >> 48);
        out[i * 8 + 7] = (uint8_t)(ctx->h[i] >> 56);
    }
}

void oneway(const uint8_t *data, size_t len, uint8_t out[32])
{
    oneway_ctx_t ctx;
    oneway_init(&ctx);
    oneway_update(&ctx, data, len);
    oneway_final(&ctx, out);
}

uint32_t ow_mix32(uint32_t x)
{
    /* reversible mix — used to contrast with true one-way compression */
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
