#ifndef ONEWAY_H
#define ONEWAY_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 2 / 18 concept: one-way hash via a simple Merkle-Damgård construction
 * built on a 256-bit Davies-Meyer compressor over a toy 64-bit block function.
 * Educational only — not for production. */

typedef struct {
    uint64_t h[4];   /* 256-bit state */
    uint64_t nbytes;
    uint8_t  buf[128];
    size_t   buflen;
} oneway_ctx_t;

void oneway_init(oneway_ctx_t *ctx);
void oneway_update(oneway_ctx_t *ctx, const uint8_t *data, size_t len);
void oneway_final(oneway_ctx_t *ctx, uint8_t out[32]);

void oneway(const uint8_t *data, size_t len, uint8_t out[32]);

/* XOR one-way "trapdoor-free" demonstration helpers */
uint32_t ow_mix32(uint32_t x); /* invertible-ish mix used to demo invert vs one-way */

#endif
