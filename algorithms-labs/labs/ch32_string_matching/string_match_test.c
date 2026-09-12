#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clrs.h"
#include "string_match.h"
#include "test.h"

static int same_positions(const size_t *a, const size_t *b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i]) {
      return 0;
    }
  }
  return 1;
}

static void check_all(TestSuite *t, const char *T, const char *P,
                      size_t exp_n, const size_t *exp) {
  size_t m1[16], m2[16], m3[16];
  size_t c1 = naive_match(T, P, m1, 16);
  size_t c2 = rabin_karp_match(T, P, m2, 16, 101, 256);
  size_t c3 = kmp_match(T, P, m3, 16);
  if (c1 != exp_n || !same_positions(m1, exp, exp_n)) {
    fprintf(stderr, "  naive fail T=%s P=%s\n", T, P);
  }
  ASSERT_EQ_INT(t, (int)c1, (int)exp_n);
  ASSERT_TRUE(t, same_positions(m1, exp, exp_n));
  ASSERT_EQ_INT(t, (int)c2, (int)exp_n);
  ASSERT_TRUE(t, same_positions(m2, exp, exp_n));
  ASSERT_EQ_INT(t, (int)c3, (int)exp_n);
  ASSERT_TRUE(t, same_positions(m3, exp, exp_n));
}

int main(void) {
  TestSuite t;
  test_init(&t);

  {
    size_t exp[] = {0, 1, 2, 3};
    check_all(&t, "aaaa", "a", 4, exp);
  }
  {
    size_t exp[] = {0, 1, 2};
    check_all(&t, "aaaa", "aa", 3, exp);
  }
  {
    size_t exp[] = {0, 3, 6};
    check_all(&t, "abcabcabcabc", "abcabc", 3, exp);
  }
  {
    size_t exp[] = {0, 4};
    check_all(&t, "aaabaaab", "aaab", 2, exp);
  }
  {
    size_t exp[] = {0, 3, 6};
    check_all(&t, "abcabcabc", "abc", 3, exp);
  }
  {
    size_t exp[] = {2};
    check_all(&t, "hello world", "llo", 1, exp);
  }
  {
    size_t exp[] = {0};
    check_all(&t, "abcdef", "xyz", 0, exp);
  }
  {
    size_t exp[] = {0};
    check_all(&t, "abc", "abc", 1, exp);
  }

  /* prefix function book example: ababaca -> 0012301 */
  {
    size_t pi[7];
    compute_prefix_function("ababaca", 7, pi);
    size_t want[] = {0, 0, 1, 2, 3, 0, 1};
    ASSERT_TRUE(&t, same_positions(pi, want, 7));
  }

  {
    size_t pi[1];
    compute_prefix_function("a", 1, pi);
    ASSERT_EQ_INT(&t, (int)pi[0], 0);
  }

  /* overlapping pattern aaa in aaaaa */
  {
    size_t exp[] = {0, 1, 2};
    check_all(&t, "aaaaa", "aaa", 3, exp);
  }

  /* forced spurious hit: with q=255, d=256 the hashes of "ab" and "ba"
   * collide (24930 mod 255 == 25185 mod 255 == 195), so the Rabin-Karp
   * memcmp verification branch must reject the false match */
  {
    const char *T = "ab", *P = "ba";
    size_t hT = 0, hP = 0;
    for (const char *c = T; *c; c++) {
      hT = (hT * 256 + (unsigned char)*c) % 255;
    }
    for (const char *c = P; *c; c++) {
      hP = (hP * 256 + (unsigned char)*c) % 255;
    }
    ASSERT_EQ_INT(&t, (int)hT, (int)hP); /* collision confirmed */
    size_t m[4];
    ASSERT_EQ_INT(&t, (int)rabin_karp_match(T, P, m, 4, 255, 256), 0);
    ASSERT_EQ_INT(&t, (int)rabin_karp_match("abz", P, m, 4, 255, 256), 0);
  }

  return test_report(&t, "string_match");
}
