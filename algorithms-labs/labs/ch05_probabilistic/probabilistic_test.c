#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "hiring.h"
#include "random.h"
#include "test.h"

static int is_permutation(const int *a, size_t n) {
  int *seen = clrs_xcalloc(n, sizeof(int));
  for (size_t i = 0; i < n; i++) {
    if (a[i] < 0 || (size_t)a[i] >= n || seen[a[i]]) {
      free(seen);
      return 0;
    }
    seen[a[i]] = 1;
  }
  free(seen);
  return 1;
}

int main(void) {
  TestSuite t;
  test_init(&t);

  clrs_srand(42u);

  /* --- randomize preserves multiset --- */
  {
    int a[10];
    for (int i = 0; i < 10; i++) {
      a[i] = i;
    }
    clrs_randomize_in_place(a, 10);
    ASSERT_TRUE(&t, is_permutation(a, 10));
  }

  /* identity under size 0/1 */
  {
    int a[1] = {7};
    clrs_randomize_in_place(a, 1);
    ASSERT_EQ_INT(&t, a[0], 7);
  }

  /* --- hiring: fixed order --- */
  {
    /* scores: c0=3, c1=1, c2=2, c3=5, c4=4
     * order 0,1,2,3,4: hire 0 (3), hire 3 (5) → 2 hires */
    int scores[] = {3, 1, 2, 5, 4};
    size_t order[] = {0, 1, 2, 3, 4};
    HiringResult r = hire_assistants(scores, 5, order, 100);
    ASSERT_EQ_INT(&t, r.interviews, 5);
    ASSERT_EQ_INT(&t, r.hires, 2);
    ASSERT_EQ_INT(&t, r.cost, 200);
  }

  {
    /* Always better: hire everyone */
    int scores[] = {1, 2, 3, 4};
    size_t order[] = {0, 1, 2, 3};
    HiringResult r = hire_assistants(scores, 4, order, 10);
    ASSERT_EQ_INT(&t, r.hires, 4);
  }

  {
    /* Always worse after first: hire only first */
    int scores[] = {5, 1, 2, 0};
    size_t order[] = {0, 1, 2, 3};
    HiringResult r = hire_assistants(scores, 4, order, 10);
    ASSERT_EQ_INT(&t, r.hires, 1);
  }

  {
    /* Empty */
    HiringResult r = hire_assistants(NULL, 0, NULL, 1);
    ASSERT_EQ_INT(&t, r.hires, 0);
  }

  /* --- randomized: expectation of hires ≈ H_n for distinct random ranks ---
   * For uniformly random permutation of distinct ranks, E[hires] = H_n.
   * n=32: H_32 ≈ 4.0585. Sample mean should be near that. */
  {
    const size_t n = 32;
    const int trials = 20000;
    int *scores = clrs_xmalloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) {
      scores[i] = (int)i; /* distinct ranks */
    }

    double sum_hires = 0.0;
    for (int tr = 0; tr < trials; tr++) {
      HiringResult r = randomized_hire_assistants(scores, n, 1);
      sum_hires += (double)r.hires;
    }
    double mean = sum_hires / (double)trials;

    /* H_32 = sum 1/i */
    double H = 0.0;
    for (size_t i = 1; i <= n; i++) {
      H += 1.0 / (double)i;
    }

    fprintf(stderr, "  E[hires] sample=%.4f  H_n=%.4f\n", mean, H);
    /* Wide band: Monte Carlo noise + rand() quality */
    ASSERT_TRUE(&t, mean > H - 0.35 && mean < H + 0.35);
    free(scores);
  }

  /* --- randomized always interviews everyone --- */
  {
    int scores[] = {10, 20, 30};
    HiringResult r = randomized_hire_assistants(scores, 3, 5);
    ASSERT_EQ_INT(&t, r.interviews, 3);
    ASSERT_TRUE(&t, r.hires >= 1 && r.hires <= 3);
    ASSERT_EQ_INT(&t, r.cost, (long long)r.hires * 5);
  }

  return test_report(&t, "probabilistic_ch05");
}
