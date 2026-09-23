#include "dp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 指派 DFS：全排列展开，维护当前代价（n<=8） */
static void assign_rec(const double *cost, int n, int depth, char *used,
                       int *perm, double cur, double *best, int *perm_best)
{
    int c;
    if (depth == n) {
        if (cur < *best - 1e-12) {
            *best = cur;
            if (perm_best) memcpy(perm_best, perm, (size_t)n * sizeof(int));
        }
        return;
    }
    for (c = 0; c < n; ++c) {
        if (used[c]) continue;
        used[c] = 1;
        perm[depth] = c;
        assign_rec(cost, n, depth + 1, used, perm,
                   cur + cost[depth * n + c], best, perm_best);
        used[c] = 0;
    }
}

double mlab_knapsack_dp(const double *w, const double *v, int n, double cap,
                        int *x_sel)
{
    int C, i, j;
    double *dp;
    char *keep;
    double best;
    if (!w || !v || n <= 0) return -1.0;
    if (fabs(cap - round(cap)) > 1e-9) return -1.0;
    C = (int)round(cap);
    if (C < 0) return -1.0;
    for (i = 0; i < n; ++i)
        if (fabs(w[i] - round(w[i])) > 1e-9 || w[i] < 0.0) return -1.0;
    if (x_sel)
        for (i = 0; i < n; ++i) x_sel[i] = 0;

    dp = (double *)calloc((size_t)C + 1, sizeof(double));
    keep = (char *)calloc((size_t)n * (size_t)(C + 1), 1);
    if (!dp || !keep) {
        free(dp);
        free(keep);
        return -1.0;
    }
    for (i = 0; i < n; ++i) {
        int wi = (int)round(w[i]);
        for (j = C; j >= wi; --j) {
            double cand = dp[j - wi] + v[i];
            if (cand > dp[j]) {
                dp[j] = cand;
                keep[i * (C + 1) + j] = 1;
            }
        }
    }
    best = dp[C];
    /* 回溯选择向量 */
    if (x_sel) {
        j = C;
        for (i = n - 1; i >= 0; --i) {
            int wi = (int)round(w[i]);
            if (j >= wi && keep[i * (C + 1) + j]) {
                x_sel[i] = 1;
                j -= wi;
            }
        }
    }
    free(dp);
    free(keep);
    return best;
}

double mlab_assign_brute(const double *cost, int n, int *perm_out)
{
    char used[8];
    int perm[8];
    int i;
    double best = 1e300;
    if (!cost || n < 1 || n > 8) return -1.0;
    for (i = 0; i < n; ++i) used[i] = 0;
    assign_rec(cost, n, 0, used, perm, 0.0, &best, perm_out);
    return best < 1e299 ? best : -1.0;
}
