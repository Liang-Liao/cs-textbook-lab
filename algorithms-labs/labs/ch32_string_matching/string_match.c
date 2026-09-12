#include "string_match.h"

#include <stdlib.h>
#include <string.h>

#include "clrs.h"

size_t naive_match(const char *T, const char *P, size_t *matches,
                   size_t max_matches) {
  size_t n = strlen(T);
  size_t m = strlen(P);
  size_t count = 0;
  if (m == 0 || n < m) {
    return 0;
  }
  for (size_t s = 0; s + m <= n; s++) {
    size_t i = 0;
    while (i < m && T[s + i] == P[i]) {
      i++;
    }
    if (i == m) {
      if (matches != NULL && count < max_matches) {
        matches[count] = s;
      }
      count++;
    }
  }
  return count;
}

size_t rabin_karp_match(const char *T, const char *P, size_t *matches,
                        size_t max_matches, unsigned q, unsigned d) {
  size_t n = strlen(T);
  size_t m = strlen(P);
  size_t count = 0;
  if (m == 0 || n < m || q < 2) {
    return 0;
  }

  unsigned long long h = 1;
  for (size_t i = 1; i < m; i++) {
    h = (h * d) % q;
  }

  unsigned long long p = 0, t = 0;
  for (size_t i = 0; i < m; i++) {
    p = (d * p + (unsigned char)P[i]) % q;
    t = (d * t + (unsigned char)T[i]) % q;
  }

  for (size_t s = 0; s + m <= n; s++) {
    if (p == t) {
      if (memcmp(T + s, P, m) == 0) {
        if (matches != NULL && count < max_matches) {
          matches[count] = s;
        }
        count++;
      }
    }
    if (s + m < n) {
      unsigned long long tval = (d * (t + q - (h * (unsigned char)T[s]) % q) +
                                 (unsigned char)T[s + m]) %
                                q;
      t = tval % q;
    }
  }
  return count;
}

void compute_prefix_function(const char *P, size_t m, size_t *pi) {
  if (m == 0) {
    return;
  }
  pi[0] = 0;
  size_t k = 0;
  for (size_t q = 1; q < m; q++) {
    while (k > 0 && P[k] != P[q]) {
      k = pi[k - 1];
    }
    if (P[k] == P[q]) {
      k++;
    }
    pi[q] = k;
  }
}

size_t kmp_match(const char *T, const char *P, size_t *matches,
                 size_t max_matches) {
  size_t n = strlen(T);
  size_t m = strlen(P);
  size_t count = 0;
  if (m == 0 || n < m) {
    return 0;
  }
  size_t *pi = clrs_xmalloc(m * sizeof(size_t));
  compute_prefix_function(P, m, pi);

  size_t q = 0;
  for (size_t i = 0; i < n; i++) {
    while (q > 0 && P[q] != T[i]) {
      q = pi[q - 1];
    }
    if (P[q] == T[i]) {
      q++;
    }
    if (q == m) {
      size_t s = i + 1 - m;
      if (matches != NULL && count < max_matches) {
        matches[count] = s;
      }
      count++;
      q = pi[m - 1];
    }
  }
  free(pi);
  return count;
}
