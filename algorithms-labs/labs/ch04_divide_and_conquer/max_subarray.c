#include "max_subarray.h"

#include <limits.h>

#include "clrs.h"

/* Find max subarray crossing the midpoint, spanning [low, high]. */
static MaxSubarray max_crossing(const long long *a, size_t low, size_t mid,
                                size_t high) {
  long long left_sum = LLONG_MIN;
  long long sum = 0;
  size_t max_left = mid;

  for (size_t i = mid + 1; i-- > low;) {
    sum += a[i];
    if (sum > left_sum) {
      left_sum = sum;
      max_left = i;
    }
  }

  long long right_sum = LLONG_MIN;
  sum = 0;
  size_t max_right = mid;

  for (size_t j = mid + 1; j <= high; j++) {
    sum += a[j];
    if (sum > right_sum) {
      right_sum = sum;
      max_right = j;
    }
  }

  MaxSubarray r;
  r.low = max_left;
  r.high = max_right;
  r.sum = left_sum + right_sum;
  return r;
}

static MaxSubarray max_subarray_rec(const long long *a, size_t low,
                                    size_t high) {
  MaxSubarray r;
  if (low == high) {
    r.low = low;
    r.high = high;
    r.sum = a[low];
    return r;
  }

  size_t mid = low + (high - low) / 2;
  MaxSubarray left = max_subarray_rec(a, low, mid);
  MaxSubarray right = max_subarray_rec(a, mid + 1, high);
  MaxSubarray cross = max_crossing(a, low, mid, high);

  if (left.sum >= right.sum && left.sum >= cross.sum) {
    return left;
  }
  if (right.sum >= left.sum && right.sum >= cross.sum) {
    return right;
  }
  return cross;
}

MaxSubarray max_subarray_dc(const long long *a, size_t n) {
  CLRS_ASSERT(a != NULL && n > 0, "max_subarray requires n >= 1");
  return max_subarray_rec(a, 0, n - 1);
}

MaxSubarray max_subarray_brute(const long long *a, size_t n) {
  CLRS_ASSERT(a != NULL && n > 0, "max_subarray requires n >= 1");

  MaxSubarray best;
  best.low = 0;
  best.high = 0;
  best.sum = a[0];

  for (size_t i = 0; i < n; i++) {
    long long sum = 0;
    for (size_t j = i; j < n; j++) {
      sum += a[j];
      if (sum > best.sum) {
        best.sum = sum;
        best.low = i;
        best.high = j;
      }
    }
  }
  return best;
}

MaxSubarray max_subarray_kadane(const long long *a, size_t n) {
  CLRS_ASSERT(a != NULL && n > 0, "max_subarray requires n >= 1");

  MaxSubarray best;
  best.low = 0;
  best.high = 0;
  best.sum = a[0];

  long long cur_sum = a[0];
  size_t cur_start = 0;

  for (size_t i = 1; i < n; i++) {
    if (a[i] > cur_sum + a[i]) {
      cur_sum = a[i];
      cur_start = i;
    } else {
      cur_sum += a[i];
    }
    if (cur_sum > best.sum) {
      best.sum = cur_sum;
      best.low = cur_start;
      best.high = i;
    }
  }
  return best;
}
