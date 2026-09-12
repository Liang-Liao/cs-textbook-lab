#include "random.h"

#include <stdlib.h>

#include "clrs.h"

void clrs_srand(uint32_t seed) { srand(seed); }

int clrs_rand_int(int low, int high) { return clrs_rand_range_int(low, high); }

static void swap_bytes(void *a, void *b, size_t size) {
  unsigned char *pa = a;
  unsigned char *pb = b;
  for (size_t i = 0; i < size; i++) {
    unsigned char t = pa[i];
    pa[i] = pb[i];
    pb[i] = t;
  }
}

void clrs_randomize_in_place(int *a, size_t n) {
  clrs_randomize_in_place_bytes(a, n, sizeof(int));
}

void clrs_randomize_in_place_bytes(void *base, size_t n, size_t size) {
  unsigned char *p = base;
  if (n < 2) {
    return;
  }
  for (size_t i = 0; i < n - 1; i++) {
    size_t j = i + (size_t)clrs_rand_below((long long)(n - i));
    if (j != i) {
      swap_bytes(p + i * size, p + j * size, size);
    }
  }
}
