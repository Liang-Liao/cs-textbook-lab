#ifndef DES_H
#define DES_H

#include <stdint.h>

/* Chapter 12: DES educational implementation.
 * 64-bit block, 56-bit effective key (8 bytes with parity). */

typedef struct {
    uint32_t subkey[16][2]; /* per round: 48-bit PC2 output split into two 24-bit halves */
} des_ctx_t;

void des_setkey(des_ctx_t *ctx, const uint8_t key[8]);
void des_encrypt_block(const des_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);
void des_decrypt_block(const des_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);

/* Convenience wrappers matching block_fn signature: key is des_ctx_t* */
void des_encrypt_block_raw(const uint8_t in[8], uint8_t out[8], const void *key);
void des_decrypt_block_raw(const uint8_t in[8], uint8_t out[8], const void *key);

/* Weak / semi-weak keys (Schneier Table 12.8 / 12.9) */
int des_is_weak_key(const uint8_t key[8]);

#endif
