#ifndef RC4_H
#define RC4_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 16: RC4 (Ron Rivest, in Schneier's book). */

typedef struct {
    uint8_t S[256];
    uint8_t i, j;
} rc4_ctx_t;

void rc4_init(rc4_ctx_t *ctx, const uint8_t *key, size_t keylen);
uint8_t rc4_byte(rc4_ctx_t *ctx);
void rc4_crypt(rc4_ctx_t *ctx, const uint8_t *in, uint8_t *out, size_t len);

#endif
