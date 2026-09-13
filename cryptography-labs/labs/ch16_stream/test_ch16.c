#include "crypto_common.h"
#include "lfsr.h"
#include "rc4.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 16: Stream Ciphers");

    /* Maximal 16-bit LFSR: measure true period of default taps 0x002D. */
    {
        lfsr16_t r;
        uint16_t start;
        uint32_t steps = 0;
        lfsr16_init(&r, 0xACE1, 0x002D);
        start = r.state;
        do {
            (void)lfsr16_bit(&r);
            steps++;
            if (steps > 65536u) break;
        } while (r.state != start);
        test_check(&s, "LFSR16 period is 2^16-1", steps == 65535u);
    }

    /* Determinism / non-trivial keystream from default taps */
    {
        lfsr16_t a, b;
        uint8_t k1[8], k2[8];
        int i, any = 0, same = 1;
        lfsr16_init(&a, 0x1234, 0);
        lfsr16_init(&b, 0x1234, 0);
        lfsr16_keystream(&a, k1, 8);
        lfsr16_keystream(&b, k2, 8);
        for (i = 0; i < 8; i++) {
            if (k1[i]) any = 1;
            if (k1[i] != k2[i]) same = 0;
        }
        test_check(&s, "LFSR keystream non-zero and deterministic",
                   any && same);
    }

    /* 32-bit Galois LFSR: reference keystream generated independently
     * (out = state&1; state = state>>1 ^ poly when out), MSB-first packing. */
    {
        lfsr32_t r;
        uint8_t ks[8] = {0};
        static const uint8_t e1[8] = {0x13,0x38,0x3d,0xb0,0x99,0x08,0x92,0x70};
        static const uint8_t e2[8] = {0x83,0x66,0x0d,0x5b,0xa7,0xe3,0x85,0x14};
        int i, b;
        lfsr32_init(&r, 0x12345678u, 0x80200003u);
        for (i = 0; i < 8; i++)
            for (b = 0; b < 8; b++)
                ks[i] = (uint8_t)((ks[i] << 1) | lfsr32_bit(&r));
        test_check(&s, "LFSR32 reference seq #1", memcmp(ks, e1, 8) == 0);

        memset(ks, 0, sizeof(ks));
        lfsr32_init(&r, 0xDEADBEEFu, 0x80000057u);
        for (i = 0; i < 8; i++)
            for (b = 0; b < 8; b++)
                ks[i] = (uint8_t)((ks[i] << 1) | lfsr32_bit(&r));
        test_check(&s, "LFSR32 reference seq #2", memcmp(ks, e2, 8) == 0);

        lfsr32_init(&r, 0, 0x80000057u);
        test_check(&s, "LFSR32 zero seed guarded to 1", r.state == 1);
    }

    /* RC4 known-answer: key "Key", plaintext "Plaintext"
     * Classic vector ciphertext BBF316E8D940AF0AD3 */
    {
        const uint8_t *key = (const uint8_t *)"Key";
        const uint8_t *pt  = (const uint8_t *)"Plaintext";
        uint8_t ct[9], expect[9] = {0xbb,0xf3,0x16,0xe8,0xd9,0x40,0xaf,0x0a,0xd3};
        rc4_ctx_t ctx;
        rc4_init(&ctx, key, 3);
        rc4_crypt(&ctx, pt, ct, 9);
        test_check_eq_hex(&s, "RC4 KAT Key/Plaintext", ct, 9, expect, 9);
    }

    /* RC4 roundtrip */
    {
        uint8_t key[16], pt[32], ct[32], rt[32];
        rc4_ctx_t e, d;
        crypto_random_bytes(key, 16);
        crypto_random_bytes(pt, 32);
        rc4_init(&e, key, 16);
        rc4_init(&d, key, 16);
        rc4_crypt(&e, pt, ct, 32);
        rc4_crypt(&d, ct, rt, 32);
        test_check(&s, "RC4 roundtrip", memcmp(pt, rt, 32) == 0);
    }

    /* RC4 second known vector: key "Wiki", plaintext "pedia" -> 1021BF0420 */
    {
        const uint8_t *key = (const uint8_t *)"Wiki";
        const uint8_t *pt  = (const uint8_t *)"pedia";
        uint8_t ct[5], expect[5] = {0x10,0x21,0xbf,0x04,0x20};
        rc4_ctx_t ctx;
        rc4_init(&ctx, key, 4);
        rc4_crypt(&ctx, pt, ct, 5);
        test_check_eq_hex(&s, "RC4 KAT Wiki/pedia", ct, 5, expect, 5);
    }

    /* Empty key must not crash */
    {
        rc4_ctx_t ctx;
        rc4_init(&ctx, NULL, 0);
        test_check(&s, "rc4 empty key is safe", 1);
    }

    return test_end(&s);
}
