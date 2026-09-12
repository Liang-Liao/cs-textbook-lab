#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "max_subarray.h"
#include "test.h"

static void fill(long long *a, size_t n, uint32_t seed, int lo, int hi) {
  int *tmp = clrs_xmalloc(n * sizeof(int));
  array_fill_random(tmp, n, seed, lo, hi);
  for (size_t i = 0; i < n; i++) {
    a[i] = tmp[i];
  }
  free(tmp);
}

static void check_agree(TestSuite *t, const long long *a, size_t n,
                        const char *name) {
  MaxSubarray dc = max_subarray_dc(a, n);
  MaxSubarray br = max_subarray_brute(a, n);
  MaxSubarray ka = max_subarray_kadane(a, n);

  if (dc.sum != br.sum) {
    fprintf(stderr, "  '%s': dc.sum=%lld brute.sum=%lld\n", name, dc.sum,
            br.sum);
  }
  if (ka.sum != br.sum) {
    fprintf(stderr, "  '%s': kadane.sum=%lld brute.sum=%lld\n", name, ka.sum,
            br.sum);
  }
  ASSERT_EQ_INT(t, dc.sum, br.sum);
  ASSERT_EQ_INT(t, ka.sum, br.sum);

  /* Verify the reported range actually sums to best.sum. */
  long long s = 0;
  for (size_t i = dc.low; i <= dc.high; i++) {
    s += a[i];
  }
  ASSERT_EQ_INT(t, s, dc.sum);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book example (CLRS Fig 4.1): max subarray A[8..11] (1-based)
   = 18,20,-7,12 sum 43 → 0-based [7..10] */
  {
    long long book[] = {13,  -3, -25, 20,  -3, -16, -23, 18,
                        20,  -7, 12,  -5,  -22, 15, -4,  7};
    size_t n = sizeof(book) / sizeof(book[0]);
    MaxSubarray r = max_subarray_dc(book, n);
    ASSERT_EQ_INT(&t, r.sum, 43);
    ASSERT_EQ_INT(&t, (int)r.low, 7);
    ASSERT_EQ_INT(&t, (int)r.high, 10);
    check_agree(&t, book, n, "book");
  }

  {
    long long one[] = {42};
    MaxSubarray r = max_subarray_dc(one, 1);
    ASSERT_EQ_INT(&t, r.sum, 42);
    ASSERT_EQ_INT(&t, (int)r.low, 0);
    ASSERT_EQ_INT(&t, (int)r.high, 0);
  }

  {
    long long allneg[] = {-5, -1, -8, -3};
    MaxSubarray r = max_subarray_dc(allneg, 4);
    ASSERT_EQ_INT(&t, r.sum, -1);
    ASSERT_EQ_INT(&t, (int)r.low, 1);
    ASSERT_EQ_INT(&t, (int)r.high, 1);
    check_agree(&t, allneg, 4, "allneg");
  }

  {
    long long allpos[] = {1, 2, 3, 4};
    MaxSubarray r = max_subarray_dc(allpos, 4);
    ASSERT_EQ_INT(&t, r.sum, 10);
    ASSERT_EQ_INT(&t, (int)r.low, 0);
    ASSERT_EQ_INT(&t, (int)r.high, 3);
  }

  {
    long long two[] = {2, -1};
    check_agree(&t, two, 2, "two");
  }

  long long rnd[256];
  fill(rnd, 256, 99u, -20, 20);
  check_agree(&t, rnd, 256, "random256");

  long long rnd2[64];
  fill(rnd2, 64, 7u, -100, 5);
  check_agree(&t, rnd2, 64, "neg-biased");

  long long rnd3[32];
  fill(rnd3, 32, 123u, 1, 50);
  check_agree(&t, rnd3, 32, "all-positive-random");

  return test_report(&t, "max_subarray");
}
