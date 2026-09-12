#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "quicksort.h"
#include "test.h"

typedef void (*sort_fn)(int *, size_t);

static void check_case(TestSuite *t, const char *name, int *data, size_t n,
                       sort_fn sort) {
  int *work = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(work, data, n);
  array_copy_int(expect, data, n);

  sort(work, n);
  qsort(expect, n, sizeof(int), clrs_cmp_int);

  int ok = array_is_sorted_int(work, n) &&
           array_same_multiset_int(work, expect, n);
  if (!ok) {
    fprintf(stderr, "  case '%s' failed\n", name);
  }
  ASSERT_TRUE(t, ok);
  free(work);
  free(expect);
}

static void check_all_variants(TestSuite *t, const char *name, int *data,
                               size_t n) {
  check_case(t, name, data, n, quicksort_int);
  quicksort_srand(1u);
  check_case(t, name, data, n, randomized_quicksort_int);
  check_case(t, name, data, n, hoare_quicksort_int);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book-style partition example: <2 8 7 1 3 5 6 4> pivot 4
   * After Lomuto with last element 4: A becomes 2 1 3 4 7 5 6 8, q=3 */
  {
    int a[] = {2, 8, 7, 1, 3, 5, 6, 4};
    size_t q = partition_int(a, 0, 7);
    ASSERT_EQ_INT(&t, (int)q, 3);
    ASSERT_EQ_INT(&t, a[3], 4);
    for (size_t i = 0; i < q; i++) {
      ASSERT_TRUE(&t, a[i] <= a[q]);
    }
    for (size_t i = q + 1; i < 8; i++) {
      ASSERT_TRUE(&t, a[i] >= a[q]);
    }
  }

  int empty[] = {0};
  int one[] = {9};
  int two[] = {2, 1};
  int two_eq[] = {5, 5};
  int basic[] = {5, 2, 4, 6, 1, 3};
  int dups[] = {3, 1, 3, 2, 1, 2, 3};
  int all_eq[] = {7, 7, 7, 7, 7};
  int neg[] = {-1, 5, -9, 0, 3, -9};
  int asc[64], desc[64], rnd[512], big[2048];
  array_fill_ascending(asc, 64);
  array_fill_descending(desc, 64);
  array_fill_random(rnd, 512, 7u, -1000, 1000);
  array_fill_random(big, 2048, 99u, -10000, 10000);

  check_all_variants(&t, "empty", empty, 0);
  check_all_variants(&t, "one", one, 1);
  check_all_variants(&t, "two", two, 2);
  check_all_variants(&t, "two_eq", two_eq, 2);
  check_all_variants(&t, "basic", basic, 6);
  check_all_variants(&t, "dups", dups, 7);
  check_all_variants(&t, "all_eq", all_eq, 5);
  check_all_variants(&t, "neg", neg, 6);
  check_all_variants(&t, "asc", asc, 64);
  check_all_variants(&t, "desc", desc, 64);
  check_all_variants(&t, "rnd512", rnd, 512);
  check_all_variants(&t, "big2048", big, 2048);

  /* Randomized partition: pivot in [p,r], partition property holds */
  {
    quicksort_srand(42u);
    int a[] = {9, 3, 7, 1, 8, 2, 5};
    size_t q = randomized_partition_int(a, 0, 6);
    ASSERT_TRUE(&t, q <= 6);
    for (size_t i = 0; i < q; i++) {
      ASSERT_TRUE(&t, a[i] <= a[q]);
    }
    for (size_t i = q + 1; i < 7; i++) {
      ASSERT_TRUE(&t, a[i] >= a[q]);
    }
  }

  return test_report(&t, "quicksort");
}
