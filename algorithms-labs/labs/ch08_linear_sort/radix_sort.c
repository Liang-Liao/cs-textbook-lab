#include "radix_sort.h"

#include <limits.h>
#include <stdlib.h>

#include "clrs.h"
#include "counting_sort.h"

static int max_value(const int *a, size_t n) {
  int m = 0;
  for (size_t i = 0; i < n; i++) {
    if (a[i] > m) {
      m = a[i];
    }
  }
  return m;
}

/* Stable sort by digit at place (1, base, base^2, ...). */
static void count_by_digit(const int *a, int *b, size_t n, int base, int place) {
  size_t *count = clrs_xcalloc((size_t)base, sizeof(size_t));

  for (size_t i = 0; i < n; i++) {
    int digit = (a[i] / place) % base;
    count[digit]++;
  }
  for (int i = 1; i < base; i++) {
    count[i] += count[i - 1];
  }
  for (size_t i = n; i-- > 0;) {
    int digit = (a[i] / place) % base;
    count[digit]--;
    b[count[digit]] = a[i];
  }
  free(count);
}

void radix_sort_int_base(int *a, size_t n, int base) {
  CLRS_ASSERT(base >= 2, "base must be >= 2");
  if (n < 2) {
    return;
  }
  for (size_t i = 0; i < n; i++) {
    CLRS_ASSERT(a[i] >= 0, "radix_sort_int requires non-negative keys");
  }

  int m = max_value(a, n);
  int *b = clrs_xmalloc(n * sizeof(int));

  for (int place = 1; m / place > 0; place *= base) {
    count_by_digit(a, b, n, base, place);
    for (size_t i = 0; i < n; i++) {
      a[i] = b[i];
    }
    if (place > INT_MAX / base) {
      break;
    }
  }
  free(b);
}

void radix_sort_int(int *a, size_t n) { radix_sort_int_base(a, n, 10); }
