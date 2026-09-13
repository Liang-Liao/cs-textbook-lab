#include "gf2n.h"

uint8_t gf2_8_xtime(uint8_t a)
{
    return (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1B : 0x00));
}

uint8_t gf2_8_mul(uint8_t a, uint8_t b)
{
    uint8_t r = 0;
    int i;
    for (i = 0; i < 8; i++) {
        if (b & 1) r ^= a;
        a = gf2_8_xtime(a);
        b >>= 1;
    }
    return r;
}

uint8_t gf2_8_inv(uint8_t a)
{
    /* a^{-1} = a^{254} */
    uint8_t r = 1;
    int i;
    if (a == 0) return 0;
    for (i = 0; i < 7; i++) {
        r = gf2_8_mul(r, r);
        r = gf2_8_mul(r, a);
    }
    r = gf2_8_mul(r, r); /* a^{127}^2 = a^{254} */
    return r;
}

uint32_t gf2_32_clmul(uint32_t a, uint32_t b)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < 32; i++) {
        if (b & (1u << i))
            r ^= a << i;
    }
    return r;
}

uint32_t gf2_32_mod(uint32_t r, uint32_t mod)
{
    /* mod is the irreducible poly degree d (highest bit set) */
    int d = 31;
    if (mod == 0) return r;
    while (d >= 0 && ((mod >> d) & 1) == 0) d--;
    while (1) {
        int rd = 31;
        while (rd >= 0 && ((r >> rd) & 1) == 0) rd--;
        if (rd < d) break;
        r ^= mod << (rd - d);
    }
    return r;
}
