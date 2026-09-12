#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "number_theory.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* mod_exp */
  {
    ASSERT_EQ_INT(&t, (int)mod_exp(7, 0, 11), 1);
    ASSERT_EQ_INT(&t, (int)mod_exp(7, 1, 11), 7);
    ASSERT_EQ_INT(&t, (int)mod_exp(2, 10, 1000), 24); /* 1024 mod 1000 */
    ASSERT_EQ_INT(&t, (int)mod_exp(3, 5, 7), 5);      /* 243 mod 7 = 5 */
    ASSERT_EQ_INT(&t, (int)mod_exp(2, 0, 5), 1);
    /* Fermat: a^{p-1} ≡ 1 (mod p) prime p=13, a=5 */
    ASSERT_EQ_INT(&t, (int)mod_exp(5, 12, 13), 1);
  }

  /* gcd */
  {
    ASSERT_EQ_INT(&t, (int)gcd_int(0, 5), 5);
    ASSERT_EQ_INT(&t, (int)gcd_int(5, 0), 5);
    ASSERT_EQ_INT(&t, (int)gcd_int(240, 46), 2); /* book example */
    ASSERT_EQ_INT(&t, (int)gcd_int(17, 5), 1);
    ASSERT_EQ_INT(&t, (int)gcd_int(-24, 36), 12);
    ASSERT_EQ_INT(&t, (int)gcd_int(100, 100), 100);
  }

  /* extended euclid book: gcd(99,78)=3 = 3*99 + (-4)*78 */
  {
    int64_t x, y;
    int64_t d = extended_euclid(99, 78, &x, &y);
    ASSERT_EQ_INT(&t, (int)d, 3);
    ASSERT_EQ_INT(&t, (int)(99 * x + 78 * y), 3);
  }

  {
    int64_t x, y;
    int64_t d = extended_euclid(30, 20, &x, &y);
    ASSERT_EQ_INT(&t, (int)d, 10);
    ASSERT_EQ_INT(&t, (int)(30 * x + 20 * y), 10);
  }

  {
    int64_t x, y;
    int64_t d = extended_euclid(35, 15, &x, &y);
    ASSERT_EQ_INT(&t, (int)d, 5);
    ASSERT_EQ_INT(&t, (int)(35 * x + 15 * y), 5);
  }

  /* modular inverse: 3 * 5 ≡ 1 (mod 7) => inv(3)=5 */
  {
    ASSERT_EQ_INT(&t, (int)modular_inverse(3, 7), 5);
    ASSERT_EQ_INT(&t, (int)modular_inverse(5, 7), 3);
    ASSERT_EQ_INT(&t, (int)modular_inverse(1, 1), 0);
    ASSERT_EQ_INT(&t, (int)modular_inverse(4, 8), -1); /* gcd=4 */
    ASSERT_EQ_INT(&t, (int)modular_inverse(10, 17), 12); /* 10*12=120=1 mod 17 */
  }

  /* CRT: x≡2 (mod 3), x≡3 (mod 5), x≡2 (mod 7) → 23
     pairwise: x≡2 (mod 3), x≡3 (mod 5) → 8 */
  {
    int64_t x = crt_pair(2, 3, 3, 5);
    ASSERT_EQ_INT(&t, (int)x, 8);
    ASSERT_EQ_INT(&t, (int)(x % 3), 2);
    ASSERT_EQ_INT(&t, (int)(x % 5), 3);
  }

  {
    int64_t x = crt_pair(2, 5, 3, 7);
    ASSERT_EQ_INT(&t, (int)(x % 5), 2);
    ASSERT_EQ_INT(&t, (int)(x % 7), 3);
  }

  return test_report(&t, "number_theory");
}
