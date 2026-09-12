/* Demo: string matching (CLRS Ch.32). */
#include <stdio.h>

#include "string_match.h"

static void show(const char *label, size_t n, const size_t *m) {
  printf("%s (%zu):", label, n);
  for (size_t i = 0; i < n; i++) {
    printf(" %zu", m[i]);
  }
  printf("\n");
}

int main(void) {
  const char *T = "abcabcabcabc";
  const char *P = "abcabc";
  size_t m[16];

  printf("=== CLRS 32.1 Naive ===\n");
  size_t c = naive_match(T, P, m, 16);
  show("matches", c, m);

  printf("\n=== CLRS 32.2 Rabin-Karp ===\n");
  c = rabin_karp_match(T, P, m, 16, 101, 256);
  show("matches", c, m);

  printf("\n=== CLRS 32.4 KMP ===\n");
  c = kmp_match(T, P, m, 16);
  show("matches", c, m);

  printf("\n=== Prefix function for P=ababaca ===\n");
  size_t pi[7];
  compute_prefix_function("ababaca", 7, pi);
  printf("pi:");
  for (int i = 0; i < 7; i++) {
    printf(" %zu", pi[i]);
  }
  printf("\n");

  return 0;
}
