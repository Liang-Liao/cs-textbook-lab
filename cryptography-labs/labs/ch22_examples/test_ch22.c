#include "crypto_common.h"
#include "envelope.h"

int main(void)
{
    test_stats_t s;
    const uint8_t *pass = (const uint8_t *)"correct horse";
    const uint8_t msg[] = "meet at dawn by the old mill";
    uint8_t sealed[128];
    uint8_t opened[128];
    size_t slen = 0, olen = 0;

    test_begin(&s, "Chapter 22: Mini Envelope");

    test_check(&s, "seal ok",
               env_seal(pass, 13, msg, sizeof(msg)-1, sealed, sizeof(sealed), &slen) == 0);
    test_check(&s, "sealed has salt+mac+ct",
               slen == env_sealed_size(sizeof(msg)-1));

    test_check(&s, "open ok",
               env_open(pass, 13, sealed, slen, opened, sizeof(opened), &olen) == 0);
    test_check(&s, "roundtrip bytes",
               olen == sizeof(msg)-1 && memcmp(opened, msg, olen) == 0);

    /* wrong password must fail (MAC mismatch) */
    {
        uint8_t bad[128];
        size_t blen = 0;
        test_check(&s, "wrong pass rejected",
                   env_open((const uint8_t *)"wrong pass!!", 12,
                            sealed, slen, bad, sizeof(bad), &blen) != 0);
    }

    /* tampered ciphertext must fail MAC */
    {
        uint8_t bad[128];
        size_t blen = 0;
        memcpy(bad, sealed, slen);
        bad[slen - 1] ^= 0x01;
        test_check(&s, "tampered ct rejected",
                   env_open(pass, 13, bad, slen, opened, sizeof(opened), &blen) != 0);
    }

    /* tampered salt must fail MAC (salt is MACed and feeds KDF) */
    {
        uint8_t bad[128];
        size_t blen = 0;
        memcpy(bad, sealed, slen);
        bad[0] ^= 0x01;
        test_check(&s, "tampered salt rejected",
                   env_open(pass, 13, bad, slen, opened, sizeof(opened), &blen) != 0);
    }

    /* tampered MAC must fail */
    {
        uint8_t bad[128];
        size_t blen = 0;
        memcpy(bad, sealed, slen);
        bad[ENV_SALT_LEN] ^= 0x01;   /* first MAC byte */
        test_check(&s, "tampered mac rejected",
                   env_open(pass, 13, bad, slen, opened, sizeof(opened), &blen) != 0);
    }

    /* truncated blob must fail */
    {
        uint8_t bad[128];
        size_t blen = 0;
        memcpy(bad, sealed, slen);
        test_check(&s, "truncated blob rejected",
                   env_open(pass, 13, bad, slen - 1, opened, sizeof(opened), &blen) != 0);
    }

    /* different salts → different ciphertexts for same message */
    {
        uint8_t s2[128];
        size_t l2 = 0;
        test_check(&s, "second seal ok",
                   env_seal(pass, 13, msg, sizeof(msg)-1, s2, sizeof(s2), &l2) == 0 &&
                   l2 == slen);
        test_check(&s, "salts differ (ct not identical)",
                   memcmp(sealed, s2, slen) != 0);
    }

    return test_end(&s);
}
