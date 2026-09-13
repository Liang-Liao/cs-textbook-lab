#ifndef FEAL_H
#define FEAL_H

#include <stdint.h>

/* Chapter 13: FEAL-8 educational implementation (structure). */

typedef struct {
    uint32_t K[8]; /* 8 round keys (32-bit), from 64-bit key */
} feal_ctx_t;

void feal8_setkey(feal_ctx_t *ctx, const uint8_t key[8]);
void feal8_encrypt(const feal_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);
void feal8_decrypt(const feal_ctx_t *ctx, const uint8_t in[8], uint8_t out[8]);

#endif
