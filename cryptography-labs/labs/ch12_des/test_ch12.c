#include "crypto_common.h"
#include "des.h"

/* Classic DES test vector (Schneier / FIPS examples):
 * key  = 0123456789ABCDEF
 * pt   = 4E6F772069732074  ("Now is t")
 * ct   = 3FA40E8A984D4815
 * (May vary by source endianness conventions; we verify self-consistency
 *  plus a widely used pair.) */

int main(void)
{
    test_stats_t s;
    des_ctx_t ctx;
    uint8_t key[8] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    uint8_t pt[8], ct[8], rt[8];

    test_begin(&s, "Chapter 12: DES");

    des_setkey(&ctx, key);

    /* FIPS-81 / common vector: key 0123456789ABCDEF pt 0123456789ABCDEF -> ct 56CC09E7CFDC4CEF */
    {
        uint8_t p[8] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
        uint8_t expect[8] = {0x56,0xcc,0x09,0xe7,0xcf,0xdc,0x4c,0xef};
        uint8_t c[8], r[8];
        des_encrypt_block(&ctx, p, c);
        test_check_eq_hex(&s, "DES encrypt known vector", c, 8, expect, 8);
        des_decrypt_block(&ctx, c, r);
        test_check(&s, "DES decrypt roundtrip", memcmp(p, r, 8) == 0);
    }

    /* All-zero key/plaintext common vector: ct 8CA64DE9C1B123A7 */
    {
        uint8_t k0[8] = {0};
        uint8_t p0[8] = {0};
        uint8_t expect[8] = {0x8c,0xa6,0x4d,0xe9,0xc1,0xb1,0x23,0xa7};
        des_ctx_t c0;
        uint8_t c[8], r[8];
        des_setkey(&c0, k0);
        des_encrypt_block(&c0, p0, c);
        test_check_eq_hex(&s, "DES all-zero vector", c, 8, expect, 8);
        des_decrypt_block(&c0, c, r);
        test_check(&s, "DES all-zero roundtrip", memcmp(p0, r, 8) == 0);
    }

    /* Random roundtrip */
    crypto_random_bytes(pt, 8);
    des_encrypt_block(&ctx, pt, ct);
    des_decrypt_block(&ctx, ct, rt);
    test_check(&s, "DES random roundtrip", memcmp(pt, rt, 8) == 0);
    test_check(&s, "DES ciphertext != plaintext", memcmp(pt, ct, 8) != 0);

    /* Weak keys */
    {
        uint8_t weak[8] = {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01};
        uint8_t strong[8] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
        test_check(&s, "detect weak key", des_is_weak_key(weak) == 1);
        test_check(&s, "normal key not weak", des_is_weak_key(strong) == 0);
    }

    return test_end(&s);
}
