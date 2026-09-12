#include "quicksort.h"

#include <stdlib.h>

#include "clrs.h"

static void swap_int(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

void quicksort_srand(uint32_t seed) { srand(seed); }

static size_t rand_index(size_t p, size_t r) {
  /* uniform in [p, r] */
  return p + (size_t)clrs_rand_below((long long)(r - p + 1));
}

/*
 * CLRS 7.1 PARTITION (Lomuto), 0-based:
 *   x = A[r]
 *   i = p  // end of "<= x" region (exclusive)
 *   for j = p .. r-1
 *     if A[j] <= x: swap A[i], A[j]; i++
 *   swap A[i], A[r]; return i
 */
size_t partition_int(int *a, size_t p, size_t r) {
  int x = a[r];
  size_t i = p;
  for (size_t j = p; j < r; j++) {
    if (a[j] <= x) {
      swap_int(&a[i], &a[j]);
      i++;
    }
  }
  swap_int(&a[i], &a[r]);
  return i;
}

/* Recurse into the smaller side; iterate the larger. Stack O(lg n). */
static void qs_rec(int *a, size_t p, size_t r) {
  while (p < r) {
    size_t q = partition_int(a, p, r);
    size_t left_len = q - p;  /* [p, q-1] */
    size_t right_len = r - q; /* [q+1, r] */
    if (left_len < right_len) {
      if (q > 0) {
        qs_rec(a, p, q - 1);
      }
      p = q + 1;
    } else {
      qs_rec(a, q + 1, r);
      r = q - 1;
    }
  }
}

void quicksort_int(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  qs_rec(a, 0, n - 1);
}

size_t randomized_partition_int(int *a, size_t p, size_t r) {
  size_t i = rand_index(p, r);
  swap_int(&a[i], &a[r]);
  return partition_int(a, p, r);
}

static void rqs_rec(int *a, size_t p, size_t r) {
  while (p < r) {
    size_t q = randomized_partition_int(a, p, r);
    size_t left_len = q - p;
    size_t right_len = r - q;
    if (left_len < right_len) {
      if (q > 0) {
        rqs_rec(a, p, q - 1);
      }
      p = q + 1;
    } else {
      rqs_rec(a, q + 1, r);
      r = q - 1;
    }
  }
}

void randomized_quicksort_int(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  rqs_rec(a, 0, n - 1);
}

/*
 * CLRS problem 7-1 HOARE-PARTITION (0-based, signed sentinels):
 *   x = A[p]; i = p-1; j = r+1
 *   do j-- until A[j] <= x; do i++ until A[i] >= x
 *   if i < j: swap; else return j
 * Quicksort recurses on [p..q] and [q+1..r].
 */
size_t hoare_partition_int(int *a, size_t p, size_t r) {
  int x = a[p];
  long i = (long)p - 1;
  long j = (long)r + 1;
  for (;;) {
    do {
      j--;
    } while (a[j] > x);
    do {
      i++;
    } while (a[i] < x);
    if (i < j) {
      swap_int(&a[i], &a[j]);
    } else {
      return (size_t)j;
    }
  }
}

static void hoare_qs_rec(int *a, size_t p, size_t r) {
  while (p < r) {
    size_t q = hoare_partition_int(a, p, r);
    /* [p..q] and [q+1..r] */
    size_t left_len = q - p + 1;
    size_t right_len = r > q ? r - q : 0;
    if (left_len < right_len) {
      hoare_qs_rec(a, p, q);
      p = q + 1;
    } else {
      if (q < r) {
        hoare_qs_rec(a, q + 1, r);
      }
      r = q;
    }
  }
}

void hoare_quicksort_int(int *a, size_t n) {
  if (n < 2) {
    return;
  }
  hoare_qs_rec(a, 0, n - 1);
}
