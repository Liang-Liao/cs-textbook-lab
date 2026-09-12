#include "order_statistics.h"

#include <stdlib.h>

#include "clrs.h"

static void swap_int(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

void order_stat_srand(uint32_t seed) { srand(seed); }

MinMax min_max_pair(const int *a, size_t n) {
  CLRS_ASSERT(n > 0, "empty array");
  MinMax r;
  r.min = a[0];
  r.max = a[0];
  for (size_t i = 1; i < n; i++) {
    if (a[i] < r.min) {
      r.min = a[i];
    }
    if (a[i] > r.max) {
      r.max = a[i];
    }
  }
  return r;
}

MinMax min_max_optimized(const int *a, size_t n) {
  CLRS_ASSERT(n > 0, "empty array");
  size_t i = 1;
  int lo, hi;
  if (n % 2 == 0) {
    if (a[0] < a[1]) {
      lo = a[0];
      hi = a[1];
    } else {
      lo = a[1];
      hi = a[0];
    }
    i = 2;
  } else {
    lo = a[0];
    hi = a[0];
    i = 1;
  }

  while (i + 1 < n) {
    int small, large;
    if (a[i] < a[i + 1]) {
      small = a[i];
      large = a[i + 1];
    } else {
      small = a[i + 1];
      large = a[i];
    }
    if (small < lo) {
      lo = small;
    }
    if (large > hi) {
      hi = large;
    }
    i += 2;
  }

  MinMax r;
  r.min = lo;
  r.max = hi;
  return r;
}

/* Lomuto partition, pivot = a[r]. Returns final pivot index. */
static size_t partition(int *a, size_t p, size_t r) {
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

static size_t randomized_partition(int *a, size_t p, size_t r) {
  /* uniform pivot index in [p, r] */
  size_t i = p + (size_t)clrs_rand_below((long long)(r - p + 1));
  swap_int(&a[i], &a[r]);
  return partition(a, p, r);
}

/* RANDOMIZED-SELECT(A, p, r, i) — i is 0-based order within [p,r]. */
static int randomized_select_rec(int *a, size_t p, size_t r, size_t i) {
  if (p == r) {
    return a[p];
  }
  size_t q = randomized_partition(a, p, r);
  size_t k = q - p; /* rank of pivot in [p,r], 0-based */
  if (i == k) {
    return a[q];
  }
  if (i < k) {
    if (q == p) {
      return a[p];
    }
    return randomized_select_rec(a, p, q - 1, i);
  }
  return randomized_select_rec(a, q + 1, r, i - k - 1);
}

int randomized_select_int(int *a, size_t n, size_t i) {
  CLRS_ASSERT(n > 0 && i < n, "invalid order statistic");
  return randomized_select_rec(a, 0, n - 1, i);
}

/* Insertion sort subrange [p..r] (inclusive). */
static void insertion_sort_range(int *a, size_t p, size_t r) {
  for (size_t j = p + 1; j <= r; j++) {
    int key = a[j];
    size_t i = j;
    while (i > p && a[i - 1] > key) {
      a[i] = a[i - 1];
      i--;
    }
    a[i] = key;
  }
}

/* Median of five elements a[p..p+4] after sorting; returns value and moves
 * the median to a[p+2] (middle). Simple: sort the five. */
static int median_of_five(int *a, size_t p) {
  insertion_sort_range(a, p, p + 4);
  return a[p + 2];
}

/*
 * CLRS 9.3 SELECT: partition around median of medians.
 * Worst-case linear time (underlying theory); implemented faithfully.
 */
static int select_rec(int *a, size_t p, size_t r, size_t i) {
  size_t n = r - p + 1;
  if (n <= 5) {
    insertion_sort_range(a, p, r);
    return a[p + i];
  }

  /* Divide into groups of 5, find each group median, move medians to front. */
  size_t num_groups = n / 5;
  for (size_t g = 0; g < num_groups; g++) {
    size_t left = p + g * 5;
    int med = median_of_five(a, left);
    /* swap median to a[p + g] */
    /* find its index in the group (median is at left+2 after sort) */
    swap_int(&a[p + g], &a[left + 2]);
    CLRS_UNUSED(med);
  }

  /* If leftover group (n % 5 != 0), also move its median */
  if (n % 5 != 0) {
    size_t left = p + num_groups * 5;
    insertion_sort_range(a, left, r);
    /* median of leftover of size m is at left + m/2 */
    size_t m = r - left + 1;
    swap_int(&a[p + num_groups], &a[left + m / 2]);
  }

  size_t med_count = num_groups + ((n % 5 != 0) ? 1 : 0);
  /* Median of medians is select of median rank among first med_count elems */
  size_t mom_rank = (med_count - 1) / 2;
  /* After select_rec, mom sits at a[p + mom_rank]. */
  select_rec(a, p, p + med_count - 1, mom_rank);
  swap_int(&a[p + mom_rank], &a[r]);
  size_t q = partition(a, p, r);

  size_t k = q - p;
  if (i == k) {
    return a[q];
  }
  if (i < k) {
    if (q == p) {
      return a[p];
    }
    return select_rec(a, p, q - 1, i);
  }
  return select_rec(a, q + 1, r, i - k - 1);
}

int select_int(int *a, size_t n, size_t i) {
  CLRS_ASSERT(n > 0 && i < n, "invalid order statistic");
  return select_rec(a, 0, n - 1, i);
}
