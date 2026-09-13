#include "rc4.h"
#include <string.h>

void rc4_init(rc4_ctx_t *ctx, const uint8_t *key, size_t keylen)
{
    int i, j = 0;
    uint8_t t;
    if (!ctx) return;
    if (!key || keylen == 0) {
        /* refuse empty key: leave a known-zero state */
        memset(ctx->S, 0, sizeof(ctx->S));
        ctx->i = 0;
        ctx->j = 0;
        return;
    }
    for (i = 0; i < 256; i++) ctx->S[i] = (uint8_t)i;
    for (i = 0; i < 256; i++) {
        j = (j + ctx->S[i] + key[i % keylen]) & 0xFF;
        t = ctx->S[i];
        ctx->S[i] = ctx->S[j];
        ctx->S[j] = t;
    }
    ctx->i = 0;
    ctx->j = 0;
}

uint8_t rc4_byte(rc4_ctx_t *ctx)
{
    uint8_t t;
    ctx->i = (uint8_t)(ctx->i + 1);
    ctx->j = (uint8_t)(ctx->j + ctx->S[ctx->i]);
    t = ctx->S[ctx->i];
    ctx->S[ctx->i] = ctx->S[ctx->j];
    ctx->S[ctx->j] = t;
    return ctx->S[(uint8_t)(ctx->S[ctx->i] + ctx->S[ctx->j])];
}

void rc4_crypt(rc4_ctx_t *ctx, const uint8_t *in, uint8_t *out, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        out[i] = (uint8_t)(in[i] ^ rc4_byte(ctx));
}
