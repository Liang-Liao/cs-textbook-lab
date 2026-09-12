#include "array.h"

#include <stdlib.h>

#include "clrs.h"

void array_fill_random(int *a, size_t n, uint32_t seed, int min_val,
                       int max_val) {
  CLRS_ASSERT(a != NULL || n == 0, "null array");
  CLRS_ASSERT(min_val <= max_val, "invalid range");

  srand(seed);
  if (n == 0) {
    return;
  }

  long long span = (long long)max_val - (long long)min_val + 1;
  for (size_t i = 0; i < n; i++) {
    a[i] = min_val + (int)clrs_rand_below(span);
  }
}

void array_fill_range(int *a, size_t n, int start) {
  for (size_t i = 0; i < n; i++) {
    a[i] = start + (int)i;
  }
}

void array_fill_ascending(int *a, size_t n) { array_fill_range(a, n, 0); }

void array_fill_descending(int *a, size_t n) {
  for (size_t i = 0; i < n; i++) {
    a[i] = (int)(n - 1 - i);
  }
}

void array_fill_constant(int *a, size_t n, int value) {
  for (size_t i = 0; i < n; i++) {
    a[i] = value;
  }
}

void array_copy_int(int *dst, const int *src, size_t n) {
  for (size_t i = 0; i < n; i++) {
    dst[i] = src[i];
  }
}

int array_is_sorted_int(const int *a, size_t n) {
  for (size_t i = 1; i < n; i++) {
    if (a[i - 1] > a[i]) {
      return 0;
    }
  }
  return 1;
}

static int cmp_int_qsort(const void *a, const void *b) {
  return clrs_cmp_int(a, b);
}

int array_same_multiset_int(const int *a, const int *b, size_t n) {
  if (n == 0) {
    return 1;
  }

  int *aa = clrs_xmalloc(n * sizeof(int));
  int *bb = clrs_xmalloc(n * sizeof(int));
  array_copy_int(aa, a, n);
  array_copy_int(bb, b, n);
  qsort(aa, n, sizeof(int), cmp_int_qsort);
  qsort(bb, n, sizeof(int), cmp_int_qsort);

  int same = 1;
  for (size_t i = 0; i < n; i++) {
    if (aa[i] != bb[i]) {
      same = 0;
      break;
    }
  }
  free(aa);
  free(bb);
  return same;
}

void array_print_int(const char *label, const int *a, size_t n) {
  if (label != NULL && label[0] != '\0') {
    printf("%s: ", label);
  }
  putchar('[');
  for (size_t i = 0; i < n; i++) {
    if (i > 0) {
      printf(", ");
    }
    printf("%d", a[i]);
  }
  printf("]\n");
}
