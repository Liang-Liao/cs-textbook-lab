#include "growth.h"

#include <math.h>

long double growth_lg(long double n) {
  if (n <= 0.0L) {
    return 0.0L;
  }
  return log2l(n);
}

long double growth_n_lg_n(long double n) {
  if (n <= 0.0L) {
    return 0.0L;
  }
  return n * log2l(n);
}

long double growth_n_pow(long double n, int k) {
  if (k < 0) {
    return 0.0L;
  }
  return powl(n, (long double)k);
}

long double growth_exp(long double base, long double n) {
  if (base <= 0.0L || n < 0.0L) {
    return 0.0L;
  }
  return powl(base, n);
}

long double growth_lg_factorial(long double n) {
  if (n < 0.0L) {
    return 0.0L;
  }
  if (n <= 1.0L) {
    return 0.0L;
  }
  /* Sum lg i = lg(n!) — exact in log domain, no overflow. */
  long double sum = 0.0L;
  long long m = (long long)n;
  for (long long i = 2; i <= m; i++) {
    sum += log2l((long double)i);
  }
  return sum;
}

long double growth_harmonic(long double n) {
  if (n <= 0.0L) {
    return 0.0L;
  }
  long double sum = 0.0L;
  long long m = (long long)n;
  for (long long i = 1; i <= m; i++) {
    sum += 1.0L / (long double)i;
  }
  return sum;
}

GrowthCompare growth_compare(long double (*f_fn)(long double n),
                             long double (*g_fn)(long double n),
                             long double n0, long double n1) {
  if (n0 <= 0.0L || n1 <= n0) {
    return GROWTH_RATIO_UNCLEAR;
  }

  /* Sample n0, 2*n0, 4*n0, ... while n <= n1 (up to 16 points). */
  enum { MAX_SAMPLES = 16 };
  long double ratios[MAX_SAMPLES];
  int count = 0;

  for (long double n = n0; n <= n1 && count < MAX_SAMPLES; n *= 2.0L) {
    long double g = g_fn(n);
    if (!(g > 0.0L)) {
      return GROWTH_RATIO_UNCLEAR;
    }
    long double f = f_fn(n);
    if (!(f >= 0.0L) || f > 1.0e300L) {
      /* skip inf/nan points at the high end */
      if (count >= 2) {
        break;
      }
      return GROWTH_RATIO_UNCLEAR;
    }
    ratios[count++] = f / g;
  }

  if (count < 3) {
    return GROWTH_RATIO_UNCLEAR;
  }

  /* Count directional moves across doublings (any strict change). */
  int down = 0, up = 0;
  for (int i = 1; i < count; i++) {
    if (ratios[i] < ratios[i - 1]) {
      down++;
    } else if (ratios[i] > ratios[i - 1]) {
      up++;
    }
  }

  long double first = ratios[0];
  long double last = ratios[count - 1];
  int steps = count - 1;

  /* f = o(g): ratio shrinks and overall drops substantially. */
  if (down * 3 >= steps * 2 && first > 0.0L && last / first < 0.5L) {
    return GROWTH_RATIO_FASTER;
  }
  /* g = o(f): ratio grows and overall rises substantially. */
  if (up * 3 >= steps * 2 && first > 0.0L && last / first > 2.0L) {
    return GROWTH_RATIO_SLOWER;
  }

  long double min = ratios[0], max = ratios[0];
  for (int i = 1; i < count; i++) {
    if (ratios[i] < min) min = ratios[i];
    if (ratios[i] > max) max = ratios[i];
  }
  if (min > 0.0L && max / min < 2.0L) {
    return GROWTH_RATIO_SAME_ORDER;
  }
  return GROWTH_RATIO_UNCLEAR;
}
