/* Demo: hiring problem and random permutation (CLRS Ch.5). */
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "hiring.h"
#include "random.h"

int main(void) {
  clrs_srand(2024u);

  const size_t n = 10;
  int scores[10];
  for (size_t i = 0; i < n; i++) {
    scores[i] = (int)(i + 1); /* distinct ranks 1..10 */
  }

  printf("=== CLRS 5.1 Hiring problem ===\n");
  printf("Candidate scores (index: score):\n  ");
  for (size_t i = 0; i < n; i++) {
    printf("[%zu]=%d ", i, scores[i]);
  }
  printf("\n");

  size_t order[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  HiringResult fixed = hire_assistants(scores, n, order, 1000);
  printf("Fixed order 0..n-1: hires=%zu cost=%lld\n", fixed.hires, fixed.cost);

  HiringResult rnd = randomized_hire_assistants(scores, n, 1000);
  printf("Randomized once:    hires=%zu cost=%lld\n", rnd.hires, rnd.cost);

  printf("\nMonte Carlo E[hires] vs H_n (n=16, 5000 trials):\n");
  const size_t m = 16;
  const int trials = 5000;
  int *s = clrs_xmalloc(m * sizeof(int));
  for (size_t i = 0; i < m; i++) {
    s[i] = (int)i;
  }
  double sum = 0.0;
  for (int tr = 0; tr < trials; tr++) {
    sum += (double)randomized_hire_assistants(s, m, 1).hires;
  }
  double H = 0.0;
  for (size_t i = 1; i <= m; i++) {
    H += 1.0 / (double)i;
  }
  printf("  sample mean = %.4f   H_16 = %.4f\n", sum / trials, H);
  free(s);

  printf("\n=== CLRS 5.3 Random permutation ===\n");
  int a[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  printf("before: ");
  for (int i = 0; i < 8; i++) {
    printf("%d ", a[i]);
  }
  clrs_randomize_in_place(a, 8);
  printf("\nafter:  ");
  for (int i = 0; i < 8; i++) {
    printf("%d ", a[i]);
  }
  printf("\n");

  return 0;
}
