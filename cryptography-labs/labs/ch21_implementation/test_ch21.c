#include "crypto_common.h"
#include "oracle.h"

int main(void)
{
    test_stats_t s;
    oracle_server_t srv;
    uint8_t msg[] = "attack-at-dawn!!"; /* 17 bytes incl. NUL → pad 7 → 24 */
    uint8_t ct[64], rec[64];
    size_t ct_len = 0, rec_len = 0;

    test_begin(&s, "Chapter 21: Implementation / Padding Oracle");

    oracle_server_init(&srv, 0xCAFEBABEDEADBEEFULL);
    oracle_encrypt(&srv, msg, 17, ct, &ct_len);
    test_check(&s, "encrypt produces blocks", ct_len == 24);
    test_check(&s, "oracle accepts good padding", oracle_padding_ok(&srv, ct, ct_len) == 1);

    {
        uint8_t bad[24];
        memcpy(bad, ct, 24);
        bad[23] ^= 0x01;
        test_check(&s, "oracle rejects corrupted pad", oracle_padding_ok(&srv, bad, 24) == 0);
    }

    /* Direct oracle behavior on hand-crafted queries (no attack involved). */
    {
        uint8_t one[8], flipped[8];
        size_t one_len = 0;
        oracle_encrypt(&srv, (const uint8_t *)"A", 1, one, &one_len);
        test_check(&s, "single block pad=7 valid",
                   one_len == 8 && oracle_padding_ok(&srv, one, 8) == 1);
        memcpy(flipped, one, 8);
        /* pad byte 0x07 -> 0x06 while the other six pad bytes stay 0x07:
         * inconsistent by construction, so the oracle must say no */
        flipped[7] ^= 0x01;
        test_check(&s, "inconsistent pad rejected",
                   oracle_padding_ok(&srv, flipped, 8) == 0);
        test_check(&s, "non-multiple length rejected",
                   oracle_padding_ok(&srv, one, 5) == 0);
        test_check(&s, "zero length rejected",
                   oracle_padding_ok(&srv, one, 0) == 0);
    }

    test_check(&s, "attack runs",
               padding_oracle_attack(&srv, ct, ct_len, rec, &rec_len) == 0);
    test_check(&s, "recovered full plaintext",
               rec_len == 17 && memcmp(rec, msg, 17) == 0);

    return test_end(&s);
}
