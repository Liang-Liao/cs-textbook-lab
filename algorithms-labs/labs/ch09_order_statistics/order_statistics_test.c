#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "order_statistics.h"
#include "test.h"

static int cmp_int(const void *a, const void *b) {
  return clrs_cmp_int(a, b);
}

static void check_select_all_ranks(TestSuite *t, const char *name, int *data,
                                   size_t n) {
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(expect, data, n);
  qsort(expect, n, sizeof(int), cmp_int);

  for (size_t i = 0; i < n; i++) {
    int *work = clrs_xmalloc(n * sizeof(int));
    array_copy_int(work, data, n);
    int v = randomized_select_int(work, n, i);
    if (v != expect[i]) {
      fprintf(stderr, "  '%s' randomized_select rank %zu: got %d expect %d\n",
              name, i, v, expect[i]);
    }
    ASSERT_EQ_INT(t, v, expect[i]);
    free(work);
  }

  for (size_t i = 0; i < n; i++) {
    int *work = clrs_xmalloc(n * sizeof(int));
    array_copy_int(work, data, n);
    int v = select_int(work, n, i);
    if (v != expect[i]) {
      fprintf(stderr, "  '%s' select rank %zu: got %d expect %d\n", name, i, v,
              expect[i]);
    }
    ASSERT_EQ_INT(t, v, expect[i]);
    free(work);
  }

  free(expect);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  order_stat_srand(42u);

  /* min/max */
  {
    int a[] = {3, 1, 4, 1, 5, 9, 2, 6};
    MinMax r = min_max_pair(a, 8);
    ASSERT_EQ_INT(&t, r.min, 1);
    ASSERT_EQ_INT(&t, r.max, 9);
    MinMax r2 = min_max_optimized(a, 8);
    ASSERT_EQ_INT(&t, r2.min, 1);
    ASSERT_EQ_INT(&t, r2.max, 9);
  }
  {
    int a[] = {7};
    MinMax r = min_max_optimized(a, 1);
    ASSERT_EQ_INT(&t, r.min, 7);
    ASSERT_EQ_INT(&t, r.max, 7);
  }
  {
    int a[] = {5, 2};
    MinMax r = min_max_optimized(a, 2);
    ASSERT_EQ_INT(&t, r.min, 2);
    ASSERT_EQ_INT(&t, r.max, 5);
  }

  /* selection */
  {
    int a[] = {2, 8, 7, 1, 3, 5, 6, 4};
    /* sorted: 1,2,3,4,5,6,7,8 */
    int *w1 = clrs_xmalloc(8 * sizeof(int));
    array_copy_int(w1, a, 8);
    ASSERT_EQ_INT(&t, randomized_select_int(w1, 8, 0), 1);
    ASSERT_EQ_INT(&t, randomized_select_int(w1, 8, 3), 4);
    ASSERT_EQ_INT(&t, randomized_select_int(w1, 8, 7), 8);
    free(w1);

    int *w2 = clrs_xmalloc(8 * sizeof(int));
    array_copy_int(w2, a, 8);
    ASSERT_EQ_INT(&t, select_int(w2, 8, 0), 1);
    ASSERT_EQ_INT(&t, select_int(w2, 8, 3), 4);
    ASSERT_EQ_INT(&t, select_int(w2, 8, 7), 8);
    free(w2);
  }

  {
    int a[] = {5, 5, 5, 5, 5};
    check_select_all_ranks(&t, "dups", a, 5);
  }

  {
    int a[64];
    array_fill_random(a, 64, 9u, -20, 20);
    check_select_all_ranks(&t, "rnd64", a, 64);
  }

  {
    int a[100];
    array_fill_ascending(a, 100);
    /* min/max on sorted */
    MinMax r = min_max_optimized(a, 100);
    ASSERT_EQ_INT(&t, r.min, 0);
    ASSERT_EQ_INT(&t, r.max, 99);
    check_select_all_ranks(&t, "asc100", a, 100);
  }

  {
    int a[37];
    array_fill_random(a, 37, 77u, 0, 1000);
    check_select_all_ranks(&t, "n37", a, 37);
  }

  /* all-equal: extreme partition skew in SELECT (groups all tie) */
  {
    int a[101];
    array_fill_constant(a, 101, 7);
    check_select_all_ranks(&t, "all_eq101", a, 101);
  }

  /* larger random sample with duplicates */
  {
    int a[1024];
    array_fill_random(a, 1024, 1234u, -50, 50);
    check_select_all_ranks(&t, "rnd1024", a, 1024);
  }

  /* descending input */
  {
    int a[400];
    array_fill_descending(a, 400);
    check_select_all_ranks(&t, "desc400", a, 400);
  }

  /* median of even length: upper of the two middles with 0-based rank n/2 */
  {
    int a[] = {1, 2, 3, 4};
    int *w = clrs_xmalloc(4 * sizeof(int));
    array_copy_int(w, a, 4);
    ASSERT_EQ_INT(&t, select_int(w, 4, 1), 2);
    ASSERT_EQ_INT(&t, select_int(w, 4, 2), 3);
    free(w);
  }

  return test_report(&t, "order_statistics");
}
