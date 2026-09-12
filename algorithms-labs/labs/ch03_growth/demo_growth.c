/* CLRS Ch.3 — print growth tables and asymptotic comparisons. */
#include <math.h>
#include <stdio.h>

#include "growth.h"

static const char *compare_name(GrowthCompare c) {
  switch (c) {
    case GROWTH_RATIO_FASTER:
      return "f = o(g)";
    case GROWTH_RATIO_SLOWER:
      return "g = o(f)  (f grows faster)";
    case GROWTH_RATIO_SAME_ORDER:
      return "f = Θ(g) plausible";
    default:
      return "unclear";
  }
}

static long double n_pow1(long double n) { return growth_n_pow(n, 1); }
static long double n_pow2(long double n) { return growth_n_pow(n, 2); }
static long double n_pow3(long double n) { return growth_n_pow(n, 3); }
static long double n_pow5(long double n) { return growth_n_pow(n, 5); }
static long double two_to_n(long double n) { return growth_exp(2.0L, n); }

static void print_value_table(void) {
  const double ns[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
  const int ncount = (int)(sizeof(ns) / sizeof(ns[0]));

  printf("=== CLRS Ch.3: common growth functions ===\n");
  printf("%8s %10s %12s %12s %12s %14s %14s\n", "n", "lg n", "n", "n lg n",
         "n^2", "n^3", "2^n");
  printf("---------------------------------------------------------------"
         "-------------\n");

  for (int i = 0; i < ncount; i++) {
    double n = ns[i];
    double two_n = (double)growth_exp(2.0L, n);
    if (n <= 32) {
      printf("%8.0f %10.2f %12.0f %12.1f %12.0f %14.0f %14.0f\n", n,
             (double)growth_lg(n), n, (double)growth_n_lg_n(n),
             (double)growth_n_pow(n, 2), (double)growth_n_pow(n, 3), two_n);
    } else {
      printf("%8.0f %10.2f %12.0f %12.1f %12.0f %14.0f %14.3e\n", n,
             (double)growth_lg(n), n, (double)growth_n_lg_n(n),
             (double)growth_n_pow(n, 2), (double)growth_n_pow(n, 3), two_n);
    }
  }
  printf("\n");
}

static void print_lg_factorial_table(void) {
  printf("=== lg(n!) vs n lg n ===\n");
  printf("%8s %14s %14s %14s %10s\n", "n", "lg(n!)", "n lg n",
         "n lg n - lg(n!)", "ratio");
  printf("-----------------------------------------------------------\n");
  const double ns[] = {10, 20, 50, 100, 200, 500, 1000};
  for (size_t i = 0; i < sizeof(ns) / sizeof(ns[0]); i++) {
    double n = ns[i];
    double lgfact = (double)growth_lg_factorial(n);
    double nlg = (double)growth_n_lg_n(n);
    printf("%8.0f %14.3f %14.3f %14.3f %10.4f\n", n, lgfact, nlg, nlg - lgfact,
           (lgfact > 0) ? nlg / lgfact : 0.0);
  }
  printf("  (n lg n = Theta(lg(n!)); ratio -> 1 as n grows)\n\n");
}

static void print_comparisons(void) {
  printf("=== Asymptotic comparisons over n in [16, 1e6] ===\n");

  struct Case {
    const char *label;
    long double (*f)(long double);
    long double (*g)(long double);
  } cases[] = {
      {"lg n          vs  n", growth_lg, n_pow1},
      {"n             vs  n lg n", n_pow1, growth_n_lg_n},
      {"n lg n        vs  n^2", growth_n_lg_n, n_pow2},
      {"n^2           vs  n^3", n_pow2, n_pow3},
      {"n^3           vs  n^5", n_pow3, n_pow5},
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    GrowthCompare c = growth_compare(cases[i].f, cases[i].g, 16.0L, 1000000.0L);
    printf("  %-24s ->  %s\n", cases[i].label, compare_name(c));
  }

  {
    GrowthCompare c = growth_compare(two_to_n, n_pow3, 8.0L, 256.0L);
    printf("  %-24s ->  %s\n", "2^n           vs  n^3", compare_name(c));
  }

  printf("\n=== H_n (harmonic) ~ ln n ===\n");
  printf("%8s %12s %12s\n", "n", "H_n", "H_n - ln n");
  const double hn[] = {10, 100, 1000, 10000};
  for (size_t i = 0; i < sizeof(hn) / sizeof(hn[0]); i++) {
    double n = hn[i];
    double H = (double)growth_harmonic(n);
    printf("%8.0f %12.6f %12.6f\n", n, H, H - log(n));
  }
  printf("  (H_n = ln n + gamma + o(1), gamma ~= 0.5772)\n");
}

int main(void) {
  print_value_table();
  print_lg_factorial_table();
  print_comparisons();
  return 0;
}
