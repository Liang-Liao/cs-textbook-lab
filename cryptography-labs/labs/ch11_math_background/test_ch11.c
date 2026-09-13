#include "crypto_common.h"
#include "gf2n.h"
#include "numthy.h"

int main(void)
{
    test_stats_t s;
    test_begin(&s, "Chapter 11: Mathematical Background");

    /* Euler totient */
    test_check(&s, "phi(1)==1", euler_phi_u64(1) == 1);
    test_check(&s, "phi(9)==6", euler_phi_u64(9) == 6);
    test_check(&s, "phi(10)==4", euler_phi_u64(10) == 4);
    test_check(&s, "phi(97)==96", euler_phi_u64(97) == 96);

    /* CRT */
    {
        uint64_t x = 0;
        /* x≡2 (mod 3), x≡3 (mod 5) => x=8 */
        test_check(&s, "CRT 2,3 / 3,5 => 8",
                   crt_u64(2, 3, 3, 5, &x) == 0 && x % 3 == 2 && x % 5 == 3);
        test_check(&s, "CRT rejects non-coprime",
                   crt_u64(1, 4, 1, 6, &x) != 0);
    }

    /* GF(2^8) AES field */
    test_check(&s, "xtime(0x57)==0xAE", gf2_8_xtime(0x57) == 0xAE);
    test_check(&s, "xtime(0x80)==0x1B", gf2_8_xtime(0x80) == 0x1B);
    test_check(&s, "mul(0x57,0x83)==0xC1", gf2_8_mul(0x57, 0x83) == 0xC1);
    test_check(&s, "mul commutes", gf2_8_mul(0x12, 0x34) == gf2_8_mul(0x34, 0x12));
    test_check(&s, "a * a^{-1} == 1", gf2_8_mul(0x53, gf2_8_inv(0x53)) == 0x01);
    test_check(&s, "inv(1)==1", gf2_8_inv(1) == 1);

    /* GF(2^32) clmul + reduce by AES-like degree-8 is wrong; use degree 16 demo */
    {
        uint32_t a = 0x1234, b = 0x5678;
        uint32_t r = gf2_32_clmul(a, b);
        /* x^16 + x^5 + x^3 + x + 1 (example irreducible for demo) */
        uint32_t mod = (1u << 16) | (1u << 5) | (1u << 3) | (1u << 1) | 1u;
        uint32_t m = gf2_32_mod(r, mod);
        test_check(&s, "clmul not equal a*b", r != a * b);
        test_check(&s, "mod degree < 16", m < (1u << 16));
        test_check(&s, "reduce(1*a)==a",
                   gf2_32_mod(gf2_32_clmul(1, 0x1234), mod) == 0x1234);
    }

    return test_end(&s);
}
