#ifndef LFSR_H
#define LFSR_H

#include <stdint.h>
#include <stddef.h>

/* Chapter 16: Linear Feedback Shift Registers (Galois/Fibonacci). */

/* 16-bit Fibonacci LFSR, taps in mask (bit 0 is first output). */
typedef struct {
    uint16_t state;
    uint16_t taps; /* default 0x002D: primitive tap mask, period 2^16-1.
                    * With bit0 shifted out the recurrence is
                    * s(n+16) = s(n) ^ s(n+2) ^ s(n+3) ^ s(n+5),
                    * i.e. characteristic poly x^16+x^5+x^3+x^2+1. */
} lfsr16_t;

void lfsr16_init(lfsr16_t *r, uint16_t seed, uint16_t taps);
uint8_t lfsr16_bit(lfsr16_t *r);
uint8_t lfsr16_byte(lfsr16_t *r);
void lfsr16_keystream(lfsr16_t *r, uint8_t *out, size_t len);

/* 32-bit Galois LFSR */
typedef struct {
    uint32_t state;
    uint32_t poly; /* nonzero feedback polynomial mask */
} lfsr32_t;

void lfsr32_init(lfsr32_t *r, uint32_t seed, uint32_t poly);
uint8_t lfsr32_bit(lfsr32_t *r);

#endif
