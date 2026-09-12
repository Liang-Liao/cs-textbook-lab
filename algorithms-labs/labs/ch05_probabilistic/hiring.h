#ifndef CLRS_HIRING_H
#define CLRS_HIRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 5.1 The hiring problem.
 * Candidates have interview scores, which may be any int (higher is better).
 * Hire cost is paid each time a new assistant is better than the current.
 */

typedef struct {
  size_t interviews; /* always n */
  size_t hires;      /* number of times we hire */
  long long cost;    /* hires * hire_cost */
} HiringResult;

/*
 * Sequential interview order given by order[]: order[k] is candidate index
 * interviewed at step k. Scores[] has n entries.
 */
HiringResult hire_assistants(const int *scores, size_t n, const size_t *order,
                             int hire_cost);

/*
 * CLRS 5.1 RANDOMIZED-HIRE-ASSISTANT: randomize order then hire.
 * Uses clrs_randomize_in_place on a copy of identity order.
 */
HiringResult randomized_hire_assistants(const int *scores, size_t n,
                                        int hire_cost);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_HIRING_H */
