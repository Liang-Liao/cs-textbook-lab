#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "merge_sort.h"
#include "test.h"

static void check_case(TestSuite *t, const char *name, int *data, size_t n) {
  int *work = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(work, data, n);
  array_copy_int(expect, data, n);

  merge_sort_int(work, n);

  qsort(expect, n, sizeof(int), clrs_cmp_int);

  int ok_sorted = array_is_sorted_int(work, n);
  int ok_multiset = array_same_multiset_int(work, expect, n);

  if (!ok_sorted) {
    fprintf(stderr, "  case '%s': result not sorted\n", name);
  }
  if (!ok_multiset) {
    fprintf(stderr, "  case '%s': elements differ from qsort\n", name);
  }

  ASSERT_TRUE(t, ok_sorted && ok_multiset);

  free(work);
  free(expect);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  int empty[] = {0};
  int one[] = {7};
  int two[] = {9, 3};
  int book[] = {5, 2, 4, 7, 1, 3, 2, 6};
  int dups[] = {4, 4, 4, 1, 1, 2};
  int neg[] = {0, -1, -100, 50, 3};
  int asc[128];
  int desc[128];
  int rnd[1024];

  array_fill_ascending(asc, 128);
  array_fill_descending(desc, 128);
  array_fill_random(rnd, 1024, 7u, -5000, 5000);

  check_case(&t, "empty", empty, 0);
  check_case(&t, "one", one, 1);
  check_case(&t, "two", two, 2);
  check_case(&t, "book", book, 8);
  check_case(&t, "duplicates", dups, 6);
  check_case(&t, "negatives", neg, 5);
  check_case(&t, "ascending", asc, 128);
  check_case(&t, "descending", desc, 128);
  check_case(&t, "random1024", rnd, 1024);

  /* merge_int unit check: two sorted runs */
  int a[] = {1, 3, 5, 2, 4, 6};
  int aux[6];
  merge_int(a, 0, 2, 5, aux);
  ASSERT_EQ_INT(&t, a[0], 1);
  ASSERT_EQ_INT(&t, a[1], 2);
  ASSERT_EQ_INT(&t, a[2], 3);
  ASSERT_EQ_INT(&t, a[3], 4);
  ASSERT_EQ_INT(&t, a[4], 5);
  ASSERT_EQ_INT(&t, a[5], 6);

  return test_report(&t, "merge_sort_int");
}
