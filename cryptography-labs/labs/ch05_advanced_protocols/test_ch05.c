#include "crypto_common.h"
#include "advproto.h"
#include "sha256.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 5: Advanced Protocols (toy)");

    /* Local SHA-256 KAT (FIPS 180) */
    {
        uint8_t d[32];
        char hex[65];
        sha256((const uint8_t *)"abc", 3, d);
        bytes_to_hex(d, 32, hex);
        test_check(&s, "sha256(abc) vector",
                   strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    }

    {
        uint8_t r[8] = {9,8,7,6,5,4,3,2};
        uint8_t m[] = "secret";
        uint8_t c[32];
        commit(r, 8, m, 6, c);
        test_check(&s, "open correct", open_check(r, 8, m, 6, c) == 1);
        m[0] = 'S';
        test_check(&s, "reject wrong message", open_check(r, 8, m, 6, c) == 0);
    }

    /* Length-prefix binding: (r="ab", m="c") != (r="a", m="bc") */
    {
        uint8_t c1[32], c2[32];
        commit((const uint8_t *)"ab", 2, (const uint8_t *)"c", 1, c1);
        commit((const uint8_t *)"a", 1, (const uint8_t *)"bc", 2, c2);
        test_check(&s, "commit binds lengths", memcmp(c1, c2, 32) != 0);
    }

    {
        zk_pub_t pub = { .p = 2147483647ULL, .g = 5, .X = 0 };
        uint64_t x = 12345;
        zk_proof_t pr;
        pub.X = mod_pow_u64(pub.g, x, pub.p);
        test_check(&s, "zk prove ok", zk_prove(&pub, x, &pr) == 0);
        test_check(&s, "zk verifies", zk_verify(&pub, &pr) == 1);
        pr.z ^= 1;
        test_check(&s, "zk rejects bad z", zk_verify(&pub, &pr) == 0);
    }

    /* Soundness: forged (A,z,c) that satisfies g^z == A*X^c but uses wrong c
     * must be rejected because FS challenge is recomputed. */
    {
        zk_pub_t pub = { .p = 2147483647ULL, .g = 5, .X = 0 };
        zk_proof_t forged;
        uint64_t x = 12345, z, c, inv, Xc;
        pub.X = mod_pow_u64(pub.g, x, pub.p);
        z = 99999;
        c = 12345; /* attacker-chosen, not H(g||X||A) */
        Xc = mod_pow_u64(pub.X, c, pub.p);
        if (mod_inv_u64(Xc, pub.p, &inv) == 0) {
            forged.z = z;
            forged.c = c;
            forged.A = mod_mul_u64(mod_pow_u64(pub.g, z, pub.p), inv, pub.p);
            test_check(&s, "zk rejects forged challenge",
                       zk_verify(&pub, &forged) == 0);
        } else {
            test_check(&s, "zk rejects forged challenge", 0);
        }
    }

    return test_end(&s);
}
