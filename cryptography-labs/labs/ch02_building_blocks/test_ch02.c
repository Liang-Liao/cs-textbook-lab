#include "crypto_common.h"
#include "oneway.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 2: Protocol Building Blocks");

    /* --- number-theoretic building blocks (from common) --- */
    test_check(&s, "gcd(12,18)==6", gcd_u64(12, 18) == 6);
    test_check(&s, "gcd(17,5)==1", gcd_u64(17, 5) == 1);

    test_check(&s, "mod_mul(3,4,5)==2", mod_mul_u64(3, 4, 5) == 2);
    test_check(&s, "mod_pow(2,10,1000)==24", mod_pow_u64(2, 10, 1000) == 24);
    test_check(&s, "Fermat 2^16 mod 17", mod_pow_u64(2, 16, 17) == 1);

    {
        uint64_t inv = 0;
        int ok = mod_inv_u64(3, 11, &inv);
        test_check(&s, "3^{-1} mod 11 == 4", ok == 0 && inv == 4);
    }

    test_check(&s, "is_prime(97)", is_prime_u64(97, 8) == 1);
    test_check(&s, "!is_prime(91)", is_prime_u64(91, 8) == 0);
    test_check(&s, "!is_prime(1)", is_prime_u64(1, 8) == 0);

    {
        uint64_t p = 0;
        int ok = random_prime_u64(100000, 200000, 8, &p);
        test_check(&s, "random_prime in range prime",
                   ok == 0 && is_prime_u64(p, 16) && p >= 100000 && p < 200000);
    }

    /* --- one-way function toy (local to this lab) --- */
    {
        uint8_t h1[32], h2[32];
        const char *a = "abc";
        const char *b = "abd";
        oneway((const uint8_t *)a, 3, h1);
        oneway((const uint8_t *)a, 3, h2);
        test_check(&s, "oneway deterministic", memcmp(h1, h2, 32) == 0);
        oneway((const uint8_t *)b, 3, h2);
        test_check(&s, "oneway avalanche", memcmp(h1, h2, 32) != 0);
    }

    {
        uint32_t x = 0xDEADBEEF;
        uint32_t y = ow_mix32(x);
        test_check(&s, "reversible mix not identity", y != x);
    }

    return test_end(&s);
}
