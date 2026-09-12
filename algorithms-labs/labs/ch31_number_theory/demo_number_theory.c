/* Demo: number-theoretic algorithms (CLRS Ch.31). */
#include <stdio.h>

#include "number_theory.h"

int main(void) {
  printf("=== CLRS 31.6 Modular exponentiation ===\n");
  printf("2^10 mod 1000 = %lld (expect 24)\n", (long long)mod_exp(2, 10, 1000));
  printf("5^12 mod 13   = %lld (Fermat, expect 1)\n",
         (long long)mod_exp(5, 12, 13));

  printf("\n=== CLRS 31.2 / 31.3 Euclid ===\n");
  printf("gcd(240,46) = %lld\n", (long long)gcd_int(240, 46));
  int64_t x, y;
  int64_t d = extended_euclid(99, 78, &x, &y);
  printf("extended_euclid(99,78): d=%lld  x=%lld y=%lld  (3*99 + -4*78=3)\n",
         (long long)d, (long long)x, (long long)y);

  printf("\n=== Modular inverse ===\n");
  printf("inv(3, 7) = %lld\n", (long long)modular_inverse(3, 7));

  printf("\n=== Chinese remainder ===\n");
  int64_t z = crt_pair(2, 3, 3, 5);
  printf("x=2 mod 3, x=3 mod 5  =>  x=%lld\n", (long long)z);

  return 0;
}
