#ifndef CLRS_GREEDY_H
#define CLRS_GREEDY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 16.1 Activity-selection.
 * Activities with start[i], finish[i], finish strictly increasing
 * after sorting by finish time. Returns indices selected (in order).
 * `selected` receives activity indices; returns count.
 */
typedef struct {
  int start;
  int finish;
} Activity;

size_t activity_select(Activity *acts, size_t n, size_t *selected_out);

/*
 * CLRS-style fractional knapsack (greedy by value/weight ratio).
 * weights[], values[] length n; capacity > 0.
 * Writes fraction of each item into take[] (0..1); returns total value.
 */
double fractional_knapsack(const double *weights, const double *values,
                           size_t n, double capacity, double *take);

/*
 * CLRS 16.3 Huffman codes.
 * freq[i] > 0 for i in [0,n). Returns sum of freq*depth (weighted path
 * length). Optionally fills code_len[i] = depth of symbol i.
 */
double huffman_wpl(const int *freq, size_t n, int *code_len_out);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_GREEDY_H */
