#include "crypto_common.h"
#include "blowfish.h"

int main(void)
{
    test_stats_t s;
    blowfish_ctx_t ctx;
    uint8_t key[] = "TESTKEY";
    uint8_t pt[8] = {0,1,2,3,4,5,6,7};
    uint8_t ct[8], rt[8];

    test_begin(&s, "Chapter 14: Blowfish (educational structure)");

    blowfish_init(&ctx, key, 7);
    blowfish_encrypt_block(&ctx, pt, ct);
    blowfish_decrypt_block(&ctx, ct, rt);
    test_check(&s, "roundtrip", memcmp(pt, rt, 8) == 0);
    test_check(&s, "ciphertext != plaintext", memcmp(pt, ct, 8) != 0);

    /* different key => different ciphertext */
    {
        blowfish_ctx_t ctx2;
        uint8_t ct2[8];
        blowfish_init(&ctx2, (const uint8_t *)"OTHERKY", 7);
        blowfish_encrypt_block(&ctx2, pt, ct2);
        test_check(&s, "key change changes ct", memcmp(ct, ct2, 8) != 0);
    }

    /* avalanche: flip 1 pt bit */
    {
        uint8_t p2[8], c2[8];
        int diff = 0, i;
        memcpy(p2, pt, 8);
        p2[0] ^= 0x01;
        blowfish_encrypt_block(&ctx, p2, c2);
        for (i = 0; i < 8; i++) diff += __builtin_popcount(ct[i] ^ c2[i]);
        test_check(&s, "avalanche >= 16 bits", diff >= 16);
    }

    /* CBC-style multi-block with local mode */
    {
        uint8_t msg[16], cbc[16], back[16];
        uint8_t iv[8] = {9,9,9,9,9,9,9,9};
        uint8_t prev[8];
        int i, j;
        for (i = 0; i < 16; i++) msg[i] = (uint8_t)(0xA0 + i);
        memcpy(prev, iv, 8);
        for (i = 0; i < 16; i += 8) {
            uint8_t blk[8];
            for (j = 0; j < 8; j++) blk[j] = msg[i + j] ^ prev[j];
            blowfish_encrypt_block(&ctx, blk, cbc + i);
            memcpy(prev, cbc + i, 8);
        }
        memcpy(prev, iv, 8);
        for (i = 0; i < 16; i += 8) {
            uint8_t blk[8];
            blowfish_decrypt_block(&ctx, cbc + i, blk);
            for (j = 0; j < 8; j++) back[i + j] = blk[j] ^ prev[j];
            memcpy(prev, cbc + i, 8);
        }
        test_check(&s, "CBC multi-block roundtrip", memcmp(msg, back, 16) == 0);
    }

    return test_end(&s);
}
