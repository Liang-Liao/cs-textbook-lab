#include "crypto_common.h"
#include "rsa.h"
#include "dh.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 19: Public-Key Algorithms");

    /* g must be a primitive root of p: g^((p-1)/f) != 1 for every prime
     * factor f of p-1. Full factorization with multiplicity:
     * 2^31-2 = 2 * 3^2 * 7 * 11 * 31 * 151 * 331. */
    {
        dh_params_t params;
        static const uint64_t fac[] = {2, 3, 3, 7, 11, 31, 151, 331};
        uint64_t prod = 1;
        size_t i;
        int all_ok = 1;
        dh_default_params(&params);
        for (i = 0; i < sizeof(fac) / sizeof(fac[0]); i++) {
            prod *= fac[i];
            if (mod_pow_u64(params.g, (params.p - 1) / fac[i], params.p) == 1)
                all_ok = 0;
        }
        test_check(&s, "p-1 factorization complete", prod == params.p - 1);
        test_check(&s, "g is a primitive root mod p", all_ok);
    }

    /* DH: shared secrets agree */
    {
        dh_params_t params;
        dh_party_t alice, bob;
        uint64_t s1, s2;
        dh_default_params(&params);
        test_check(&s, "DH derive A", dh_derive_public(&alice, &params) == 0);
        test_check(&s, "DH derive B", dh_derive_public(&bob, &params) == 0);
        s1 = dh_shared_secret(bob.pub, alice.priv, &params);
        s2 = dh_shared_secret(alice.pub, bob.priv, &params);
        test_check(&s, "DH shared secrets match", s1 == s2 && s1 != 0);
        test_check(&s, "DH secrets not equal to publics",
                   s1 != alice.pub && s1 != bob.pub);
    }

    /* RSA numeric: c = m^e mod n; m = c^d mod n */
    {
        rsa_keypair_t kp;
        uint64_t m = 42;
        uint64_t c, m2;
        int ok = rsa_generate(&kp, 200, 400, 65537);
        test_check(&s, "RSA keygen", ok == 0);
        if (ok == 0) {
            c = rsa_public(m, &kp);
            m2 = rsa_private(c, &kp);
            test_check(&s, "RSA encrypt/decrypt 42", m2 == m);
            test_check(&s, "RSA ciphertext different", c != m);
        }
    }

    /* RSA with small e=17 */
    {
        rsa_keypair_t kp;
        uint64_t m = 0x123456;
        int ok = rsa_generate(&kp, 500, 800, 17);
        test_check(&s, "RSA keygen e=17", ok == 0);
        if (ok == 0 && m < kp.n)
            test_check(&s, "RSA e=17 roundtrip", rsa_private(rsa_public(m, &kp), &kp) == m);
    }

    /* RSA educational byte encoding (simplified padding; uint64 modulus) */
    {
        rsa_keypair_t kp;
        uint8_t msg[] = {0x42};
        uint8_t ct[8], pt[8];
        size_t ct_len = 0, pt_len = 0;
        /* primes ~1.2e5..2e5 => n ~ 1.4e10..4e10 (5 bytes) so PS has room */
        int ok = rsa_generate(&kp, 120000, 200000, 65537);
        test_check(&s, "RSA keygen for byte API", ok == 0);
        if (ok == 0) {
            int e = rsa_public_bytes(msg, 1, ct, &ct_len, &kp);
            test_check(&s, "RSA public_bytes", e == 0);
            if (e == 0) {
                int d = rsa_private_bytes(ct, ct_len, pt, &pt_len, &kp);
                test_check(&s, "RSA private_bytes",
                           d == 0 && pt_len == 1 && pt[0] == msg[0]);
            }
        }
    }

    return test_end(&s);
}
