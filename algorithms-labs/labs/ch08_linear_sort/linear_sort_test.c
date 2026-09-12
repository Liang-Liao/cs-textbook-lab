#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "bucket_sort.h"
#include "clrs.h"
#include "counting_sort.h"
#include "radix_sort.h"
#include "test.h"

static void check_counting(TestSuite *t, const char *name, const int *data,
                           size_t n, int k) {
  int *b = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(expect, data, n);
  counting_sort_int(data, b, n, k);
  qsort(expect, n, sizeof(int), clrs_cmp_int);
  ASSERT_TRUE(t, array_is_sorted_int(b, n) &&
                     array_same_multiset_int(b, expect, n));
  if (!array_is_sorted_int(b, n) ||
      !array_same_multiset_int(b, expect, n)) {
    fprintf(stderr, "  counting '%s' failed\n", name);
  }
  free(b);
  free(expect);
}

static void check_radix(TestSuite *t, const char *name, int *data, size_t n) {
  int *work = clrs_xmalloc(n * sizeof(int));
  int *expect = clrs_xmalloc(n * sizeof(int));
  array_copy_int(work, data, n);
  array_copy_int(expect, data, n);
  radix_sort_int(work, n);
  qsort(expect, n, sizeof(int), clrs_cmp_int);
  ASSERT_TRUE(t, array_is_sorted_int(work, n) &&
                     array_same_multiset_int(work, expect, n));
  if (!array_is_sorted_int(work, n) ||
      !array_same_multiset_int(work, expect, n)) {
    fprintf(stderr, "  radix '%s' failed\n", name);
  }
  free(work);
  free(expect);
}

static int cmp_double(const void *a, const void *b) {
  double x = *(const double *)a;
  double y = *(const double *)b;
  return (x > y) - (x < y);
}

static int doubles_sorted(const double *a, size_t n) {
  for (size_t i = 1; i < n; i++) {
    if (a[i - 1] > a[i]) {
      return 0;
    }
  }
  return 1;
}

static void check_bucket(TestSuite *t, double *data, size_t n) {
  double *work = clrs_xmalloc(n * sizeof(double));
  double *expect = clrs_xmalloc(n * sizeof(double));
  memcpy(work, data, n * sizeof(double));
  memcpy(expect, data, n * sizeof(double));
  bucket_sort_double(work, n);
  qsort(expect, n, sizeof(double), cmp_double);
  int ok = 1;
  for (size_t i = 0; i < n; i++) {
    if (fabs(work[i] - expect[i]) > 1e-15) {
      ok = 0;
    }
  }
  if (!ok) {
    fprintf(stderr, "  bucket mismatch\n");
  }
  ASSERT_TRUE(t, ok && doubles_sorted(work, n));
  free(work);
  free(expect);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book Fig 8.2 style */
  {
    int a[] = {2, 5, 3, 0, 2, 3, 0, 3};
    int b[8];
    counting_sort_int(a, b, 8, 5);
    ASSERT_EQ_INT(&t, b[0], 0);
    ASSERT_EQ_INT(&t, b[1], 0);
    ASSERT_EQ_INT(&t, b[2], 2);
    ASSERT_EQ_INT(&t, b[3], 2);
    ASSERT_EQ_INT(&t, b[4], 3);
    ASSERT_EQ_INT(&t, b[5], 3);
    ASSERT_EQ_INT(&t, b[6], 3);
    ASSERT_EQ_INT(&t, b[7], 5);
    /* input unchanged */
    ASSERT_EQ_INT(&t, a[0], 2);
  }

  {
    int one[] = {7};
    int b[1];
    counting_sort_int(one, b, 1, 7);
    ASSERT_EQ_INT(&t, b[0], 7);

    int empty_b[1];
    counting_sort_int(NULL, empty_b, 0, 0);
  }

  {
    int rnd[200];
    array_fill_random(rnd, 200, 11u, 0, 50);
    check_counting(&t, "rnd", rnd, 200, 50);

    int same[30];
    array_fill_constant(same, 30, 3);
    check_counting(&t, "same", same, 30, 3);
  }

  /* radix */
  {
    int a[] = {329, 457, 657, 839, 436, 720, 355};
    radix_sort_int(a, 7);
    int expect[] = {329, 355, 436, 457, 657, 720, 839};
    ASSERT_TRUE(&t, array_same_multiset_int(a, expect, 7) &&
                        array_is_sorted_int(a, 7));
  }

  {
    int a[] = {0, 1, 2, 3, 4, 5};
    check_radix(&t, "small", a, 6);
    int big[300];
    array_fill_random(big, 300, 22u, 0, 99999);
    check_radix(&t, "big", big, 300);
    int zeros[10];
    array_fill_constant(zeros, 10, 0);
    check_radix(&t, "zeros", zeros, 10);
  }

  /* bucket [0,1) */
  {
    double a[] = {0.78, 0.17, 0.39, 0.26, 0.72, 0.94, 0.21, 0.12, 0.23, 0.68};
    bucket_sort_double(a, 10);
    ASSERT_TRUE(&t, doubles_sorted(a, 10));
    ASSERT_TRUE(&t, fabs(a[0] - 0.12) < 1e-12);
    ASSERT_TRUE(&t, fabs(a[9] - 0.94) < 1e-12);

    double rnd[128];
    srand(33u);
    for (size_t i = 0; i < 128; i++) {
      rnd[i] = (double)rand() / ((double)RAND_MAX + 1.0);
    }
    check_bucket(&t, rnd, 128);
  }

  return test_report(&t, "linear_sort");
}
