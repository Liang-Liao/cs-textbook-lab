#ifndef BLOWFISH_H
#define BLOWFISH_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 14: Blowfish educational implementation (Schneier). */

typedef struct {
    uint32_t P[18];
    uint32_t S[4][256];
} blowfish_ctx_t;

/* NULL/empty key is refused: ctx is left in a known-zero, unusable state. */
void blowfish_init(blowfish_ctx_t *ctx, const uint8_t *key, size_t keylen);
void blowfish_encrypt_block(const blowfish_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);
void blowfish_decrypt_block(const blowfish_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);

void blowfish_encrypt_raw(const uint8_t in[8], uint8_t out[8], const void *key);
void blowfish_decrypt_raw(const uint8_t in[8], uint8_t out[8], const void *key);

#endif
