#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "insertion_sort.h"
#include "test.h"

static void check_case(TestSuite *t, const char *name, int *data, size_t n,
                       void (*sort)(int *, size_t)) {
  int *work = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(work, data, n);
  array_copy_int(expect, data, n);

  sort(work, n);

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
  int one[] = {42};
  int two_asc[] = {1, 2};
  int two_desc[] = {2, 1};
  int basic[] = {5, 2, 4, 6, 1, 3};
  int dups[] = {3, 1, 3, 2, 1, 2, 3};
  int neg[] = {-1, 5, -9, 0, 3, -9};
  int asc[64];
  int desc[64];
  int same[32];
  int rnd[512];

  array_fill_ascending(asc, 64);
  array_fill_descending(desc, 64);
  array_fill_constant(same, 32, 7);
  array_fill_random(rnd, 512, 42u, -1000, 1000);

  check_case(&t, "empty", empty, 0, insertion_sort_int);
  check_case(&t, "one", one, 1, insertion_sort_int);
  check_case(&t, "two_asc", two_asc, 2, insertion_sort_int);
  check_case(&t, "two_desc", two_desc, 2, insertion_sort_int);
  check_case(&t, "basic", basic, 6, insertion_sort_int);
  check_case(&t, "duplicates", dups, 7, insertion_sort_int);
  check_case(&t, "negatives", neg, 6, insertion_sort_int);
  check_case(&t, "ascending", asc, 64, insertion_sort_int);
  check_case(&t, "descending", desc, 64, insertion_sort_int);
  check_case(&t, "constant", same, 32, insertion_sort_int);
  check_case(&t, "random512", rnd, 512, insertion_sort_int);

  /* Book example: <5 2 4 6 1 3> → <1 2 3 4 5 6> */
  int book[] = {5, 2, 4, 6, 1, 3};
  insertion_sort_int(book, 6);
  ASSERT_EQ_INT(&t, book[0], 1);
  ASSERT_EQ_INT(&t, book[1], 2);
  ASSERT_EQ_INT(&t, book[2], 3);
  ASSERT_EQ_INT(&t, book[3], 4);
  ASSERT_EQ_INT(&t, book[4], 5);
  ASSERT_EQ_INT(&t, book[5], 6);

  return test_report(&t, "insertion_sort_int");
}
