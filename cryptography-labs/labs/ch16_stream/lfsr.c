#include "lfsr.h"

void lfsr16_init(lfsr16_t *r, uint16_t seed, uint16_t taps)
{
    r->state = seed ? seed : 1;
    /* Default: primitive x^16+x^14+x^13+x^11+1 => feedback of bits 0,2,3,5
     * when the register is stored little-endian (bit0 = output). */
    r->taps = taps ? taps : 0x002D;
}

uint8_t lfsr16_bit(lfsr16_t *r)
{
    uint8_t out = (uint8_t)(r->state & 1);
    uint16_t fb = 0;
    {
        uint16_t x = (uint16_t)(r->state & r->taps);
        while (x) {
            fb ^= (uint16_t)(x & 1);
            x >>= 1;
        }
    }
    r->state = (uint16_t)((r->state >> 1) | (fb << 15));
    return out;
}

uint8_t lfsr16_byte(lfsr16_t *r)
{
    uint8_t b = 0;
    int i;
    for (i = 0; i < 8; i++)
        b = (uint8_t)((b << 1) | lfsr16_bit(r));
    return b;
}

void lfsr16_keystream(lfsr16_t *r, uint8_t *out, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++)
        out[i] = lfsr16_byte(r);
}

void lfsr32_init(lfsr32_t *r, uint32_t seed, uint32_t poly)
{
    r->state = seed ? seed : 1;
    r->poly = poly;
}

uint8_t lfsr32_bit(lfsr32_t *r)
{
    uint8_t out = (uint8_t)(r->state & 1);
    if (r->state & 1)
        r->state = (r->state >> 1) ^ r->poly;
    else
        r->state >>= 1;
    return out;
}
