#ifndef CLRS_DP_H
#define CLRS_DP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.14 Dynamic programming.
 */

/* CLRS 15.1 rod cutting: price[i-1] = p_i for length i, n = rod length.
 * Bottom-up cut_rod. Returns max revenue. */
long long rod_cut_bottom_up(const int *price, int n);

/* Memoized top-down. scratch arrays of length n+1 provided or NULL to allocate. */
long long rod_cut_memo(const int *price, int n);

/*
 * CLRS 15.2 Matrix-chain multiplication.
 * dims: dims[0..k] where matrix i has size dims[i] x dims[i+1] for i in [0,k).
 * k = n-1 if we have n matrices, dims length = n+1.
 * Returns minimum number of scalar multiplications.
 * Optional splits: splits[i][j] = best k for Ai..Aj (1-based book style).
 *   We use 0-based: splits is n x n, splits[i][j] for i<=j.
 */
long long matrix_chain_min(const int *dims, size_t n, int *splits /* n*n or NULL */);

/*
 * CLRS 15.4 Longest common subsequence.
 * Returns LCS length; optional lcs_out receives one LCS (as ints, max maxn).
 */
size_t lcs_length(const char *x, const char *y, char *lcs_out, size_t maxn);

/*
 * CLRS 15.5 Optimal binary search tree.
 * keys[0..n-1] sorted distinct; p[i] = search probability of key i;
 * q[i] = probability of dummy key d_i (i = 0..n), so n+1 q's.
 * Returns minimum expected search cost. root_out optional n*n:
 *   root[i][j] = 0-based index of root of subtree ki..kj (i<=j).
 */
double optimal_bst(const double *p, const double *q, size_t n,
                   int *root_out /* n*n or NULL */);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_DP_H */
