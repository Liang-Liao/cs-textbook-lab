#ifndef CLRS_H
#define CLRS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define CLRS_UNUSED(x) ((void)(x))

#define CLRS_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "CLRS_ASSERT failed: %s (%s:%d)\n", (msg), __FILE__,     \
              __LINE__);                                                       \
      abort();                                                                 \
    }                                                                          \
  } while (0)

/* Uniform long long in [0, range). Combines rand() draws when range
 * exceeds RAND_MAX+1 (MinGW: 32768) and rejects to remove modulo bias. */
static inline long long clrs_rand_below(long long range) {
  CLRS_ASSERT(range >= 1, "rand range must be positive");
  long long r, cap, limit;
  do {
    r = 0;
    cap = 1;
    while (cap < range) {
      r = r * (RAND_MAX + 1LL) + rand();
      cap *= (RAND_MAX + 1LL);
    }
    limit = cap - cap % range;
  } while (r >= limit);
  return r % range;
}

/* Uniform int in [low, high]. */
static inline int clrs_rand_range_int(int low, int high) {
  CLRS_ASSERT(low <= high, "invalid rand range");
  long long span = (long long)high - (long long)low + 1;
  return (int)((long long)low + clrs_rand_below(span));
}

static inline int clrs_cmp_int(const void *a, const void *b) {
  int x = *(const int *)a;
  int y = *(const int *)b;
  return (x > y) - (x < y);
}

static inline void *clrs_xmalloc(size_t n) {
  void *p = malloc(n);
  if (n > 0) {
    CLRS_ASSERT(p != NULL, "out of memory");
  }
  return p;
}

static inline void *clrs_xcalloc(size_t n, size_t size) {
  void *p = calloc(n, size);
  if (n > 0 && size > 0) {
    CLRS_ASSERT(p != NULL, "out of memory");
  }
  return p;
}

static inline void *clrs_xrealloc(void *ptr, size_t n) {
  void *p = realloc(ptr, n);
  if (n > 0) {
    CLRS_ASSERT(p != NULL, "out of memory");
  }
  return p;
}

#endif /* CLRS_H */
