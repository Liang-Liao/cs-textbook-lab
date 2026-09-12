#include <math.h>

#include "growth.h"
#include "test.h"

static long double n_pow1(long double n) { return growth_n_pow(n, 1); }
static long double n_pow2(long double n) { return growth_n_pow(n, 2); }
static long double n_pow3(long double n) { return growth_n_pow(n, 3); }
static long double two_to_n(long double n) { return growth_exp(2.0L, n); }

static int feq(long double a, long double b, long double eps) {
  long double d = a - b;
  if (d < 0) d = -d;
  return d <= eps;
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* --- elementary values --- */
  ASSERT_TRUE(&t, feq(growth_lg(1.0L), 0.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_lg(2.0L), 1.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_lg(1024.0L), 10.0L, 1e-10L));
  ASSERT_TRUE(&t, feq(growth_lg(0.0L), 0.0L, 1e-12L));

  ASSERT_TRUE(&t, feq(growth_n_lg_n(1.0L), 0.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_n_lg_n(2.0L), 2.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_n_lg_n(4.0L), 8.0L, 1e-12L));

  ASSERT_TRUE(&t, feq(growth_n_pow(3.0L, 0), 1.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_n_pow(3.0L, 2), 9.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_n_pow(5.0L, 3), 125.0L, 1e-12L));

  ASSERT_TRUE(&t, feq(growth_exp(2.0L, 0.0L), 1.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_exp(2.0L, 10.0L), 1024.0L, 1e-8L));

  /* --- lg(n!) --- */
  ASSERT_TRUE(&t, feq(growth_lg_factorial(0.0L), 0.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_lg_factorial(1.0L), 0.0L, 1e-12L));
  /* lg(2!) = 1 */
  ASSERT_TRUE(&t, feq(growth_lg_factorial(2.0L), 1.0L, 1e-12L));
  /* lg(5!) = lg(120) */
  ASSERT_TRUE(&t, feq(growth_lg_factorial(5.0L), log2l(120.0L), 1e-10L));

  /* n lg n / lg(n!) -> 1 slowly; Stirling: ~1.17 at n=1000 */
  {
    long double n = 1000.0L;
    long double lgf = growth_lg_factorial(n);
    long double nlg = growth_n_lg_n(n);
    long double ratio = nlg / lgf;
    ASSERT_TRUE(&t, ratio > 1.0L && ratio < 1.30L);
  }

  /* --- harmonic --- */
  ASSERT_TRUE(&t, feq(growth_harmonic(1.0L), 1.0L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_harmonic(2.0L), 1.5L, 1e-12L));
  ASSERT_TRUE(&t, feq(growth_harmonic(4.0L), 1.0L + 0.5L + 1.0L / 3.0L + 0.25L,
                      1e-12L));

  /* --- comparisons (CLRS 3.1 ordering) --- */
  ASSERT_EQ_INT(&t, growth_compare(growth_lg, n_pow1, 16, 1000000),
                GROWTH_RATIO_FASTER);
  ASSERT_EQ_INT(&t, growth_compare(n_pow1, growth_n_lg_n, 16, 1000000),
                GROWTH_RATIO_FASTER);
  ASSERT_EQ_INT(&t, growth_compare(growth_n_lg_n, n_pow2, 16, 1000000),
                GROWTH_RATIO_FASTER);
  ASSERT_EQ_INT(&t, growth_compare(n_pow2, n_pow3, 16, 1000000),
                GROWTH_RATIO_FASTER);
  ASSERT_EQ_INT(&t, growth_compare(two_to_n, n_pow3, 8, 256),
                GROWTH_RATIO_SLOWER); /* 2^n grows faster */
  ASSERT_EQ_INT(&t, growth_compare(n_pow2, n_pow2, 16, 1000000),
                GROWTH_RATIO_SAME_ORDER);

  /* Same-order: n vs n (identity) */
  ASSERT_EQ_INT(&t, growth_compare(n_pow1, n_pow1, 8, 1000),
                GROWTH_RATIO_SAME_ORDER);

  return test_report(&t, "growth");
}
