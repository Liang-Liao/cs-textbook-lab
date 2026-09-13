#include "crypto_common.h"
#include "feal.h"

int main(void)
{
    test_stats_t s;
    feal_ctx_t ctx;
    uint8_t key[8] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    uint8_t pt[8] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77};
    uint8_t ct[8], rt[8];

    test_begin(&s, "Chapter 13: FEAL-8 (educational)");

    feal8_setkey(&ctx, key);
    feal8_encrypt(&ctx, pt, ct);
    feal8_decrypt(&ctx, ct, rt);
    test_check(&s, "roundtrip", memcmp(pt, rt, 8) == 0);
    test_check(&s, "ct != pt", memcmp(pt, ct, 8) != 0);

    {
        feal_ctx_t c2;
        uint8_t ct2[8];
        feal8_setkey(&c2, (const uint8_t *)"ABCDEFGH");
        feal8_encrypt(&c2, pt, ct2);
        test_check(&s, "different key", memcmp(ct, ct2, 8) != 0);
    }

    {
        uint8_t p2[8], c2[8];
        int i, bits = 0;
        memcpy(p2, pt, 8);
        p2[7] ^= 1;
        feal8_encrypt(&ctx, p2, c2);
        for (i = 0; i < 8; i++) bits += __builtin_popcount(ct[i] ^ c2[i]);
        test_check(&s, "some avalanche", bits >= 4);
    }

    return test_end(&s);
}
