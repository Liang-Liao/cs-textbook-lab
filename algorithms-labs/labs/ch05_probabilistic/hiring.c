#include "hiring.h"

#include <stdlib.h>

#include "clrs.h"
#include "random.h"

HiringResult hire_assistants(const int *scores, size_t n, const size_t *order,
                             int hire_cost) {
  HiringResult r = {0, 0, 0};
  if (n == 0) {
    return r;
  }

  /* CLRS HIRE-ASSISTANT always hires candidate 1, whatever its score */
  size_t first = order[0];
  CLRS_ASSERT(first < n, "order index out of range");
  int best = scores[first];
  r.interviews = n;
  r.hires = 1;
  r.cost = hire_cost;

  for (size_t k = 1; k < n; k++) {
    size_t i = order[k];
    CLRS_ASSERT(i < n, "order index out of range");
    if (scores[i] > best) {
      best = scores[i];
      r.hires++;
      r.cost += hire_cost;
    }
  }
  return r;
}

HiringResult randomized_hire_assistants(const int *scores, size_t n,
                                        int hire_cost) {
  size_t *order = clrs_xmalloc(n * sizeof(size_t));
  for (size_t i = 0; i < n; i++) {
    order[i] = i;
  }
  clrs_randomize_in_place_bytes(order, n, sizeof(size_t));
  HiringResult r = hire_assistants(scores, n, order, hire_cost);
  free(order);
  return r;
}
