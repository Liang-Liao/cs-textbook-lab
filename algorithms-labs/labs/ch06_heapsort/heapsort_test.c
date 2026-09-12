#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "heapsort.h"
#include "test.h"

static void check_sort_case(TestSuite *t, const char *name, int *data,
                            size_t n) {
  int *work = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(work, data, n);
  array_copy_int(expect, data, n);

  heapsort_int(work, n);
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

static int is_max_heap(const int *a, size_t n) {
  for (size_t i = 0; i < n; i++) {
    size_t l = heap_left(i);
    size_t r = heap_right(i);
    if (l < n && a[l] > a[i]) {
      return 0;
    }
    if (r < n && a[r] > a[i]) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* index helpers */
  ASSERT_EQ_INT(&t, (int)heap_parent(1), 0);
  ASSERT_EQ_INT(&t, (int)heap_parent(2), 0);
  ASSERT_EQ_INT(&t, (int)heap_left(0), 1);
  ASSERT_EQ_INT(&t, (int)heap_right(0), 2);

  /* Book example (CLRS Fig 6.1 style) */
  {
    int a[] = {4, 1, 3, 2, 16, 9, 10, 14, 8, 7};
    size_t n = 10;
    build_max_heap(a, n);
    ASSERT_TRUE(&t, is_max_heap(a, n));
    ASSERT_EQ_INT(&t, a[0], 16);
  }

  /* heapsort cases */
  {
    int empty[] = {0};
    int one[] = {5};
    int two[] = {2, 1};
    int basic[] = {5, 2, 4, 6, 1, 3};
    int dups[] = {3, 1, 3, 2, 1, 2, 3};
    int neg[] = {-1, 5, -9, 0, 3};
    int asc[64], desc[64], rnd[512];
    array_fill_ascending(asc, 64);
    array_fill_descending(desc, 64);
    array_fill_random(rnd, 512, 6u, -1000, 1000);

    check_sort_case(&t, "empty", empty, 0);
    check_sort_case(&t, "one", one, 1);
    check_sort_case(&t, "two", two, 2);
    check_sort_case(&t, "basic", basic, 6);
    check_sort_case(&t, "dups", dups, 7);
    check_sort_case(&t, "neg", neg, 5);
    check_sort_case(&t, "asc", asc, 64);
    check_sort_case(&t, "desc", desc, 64);
    check_sort_case(&t, "rnd512", rnd, 512);
  }

  /* After heapsort, array is sorted; max-heap property no longer required */
  {
    int a[] = {1, 3, 2};
    heapsort_int(a, 3);
    ASSERT_EQ_INT(&t, a[0], 1);
    ASSERT_EQ_INT(&t, a[1], 2);
    ASSERT_EQ_INT(&t, a[2], 3);
  }

  return test_report(&t, "heapsort");
}
