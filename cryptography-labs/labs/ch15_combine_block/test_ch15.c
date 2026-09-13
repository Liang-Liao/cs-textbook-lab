#include "crypto_common.h"
#include "combine.h"

int main(void)
{
    test_stats_t s;
    uint8_t pt[8] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef};
    uint8_t ct[8], rt[8];
    const uint64_t k1 = 0x1111111111111111ULL;
    const uint64_t k2 = 0x2222222222222222ULL;
    const uint64_t k3 = 0x3333333333333333ULL;

    test_begin(&s, "Chapter 15: Combining Block Algorithms");

    double_encrypt(pt, ct, k1, k2);
    double_decrypt(ct, rt, k1, k2);
    test_check(&s, "double encrypt roundtrip", memcmp(pt, rt, 8) == 0);
    {
        uint8_t single[8];
        mini_encrypt_block(pt, single, &k1);
        test_check(&s, "double != single", memcmp(ct, single, 8) != 0);
    }

    /* single-key double encryption is NOT twice as strong (k1=k2 => E^2) */
    {
        uint8_t c2[8], r2[8];
        double_encrypt(pt, c2, k1, k1);
        double_decrypt(c2, r2, k1, k1);
        test_check(&s, "same-key double roundtrip", memcmp(pt, r2, 8) == 0);
    }

    triple_encrypt_ede(pt, ct, k1, k2, k3);
    triple_decrypt_ede(ct, rt, k1, k2, k3);
    test_check(&s, "triple EDE roundtrip", memcmp(pt, rt, 8) == 0);

    /* MITM recovers a pair of reduced-width keys */
    {
        const uint64_t a = 0x0ABC & 0xFFF; /* 12-bit */
        const uint64_t b = 0x0531 & 0xFFF;
        uint8_t c[8], check[8];
        mitm_result_t res;
        double_encrypt(pt, c, a, b);
        test_check(&s, "mitm runs", mitm_double(pt, c, 12, &res) == 0);
        test_check(&s, "mitm finds some key pair", res.found == 1);
        if (res.found) {
            double_encrypt(pt, check, res.k1, res.k2);
            test_check(&s, "mitm pair decrypts ct", memcmp(check, c, 8) == 0);
        }
    }

    return test_end(&s);
}
