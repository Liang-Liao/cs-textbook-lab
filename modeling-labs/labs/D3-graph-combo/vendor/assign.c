#include "assign.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 未指派行在剩余列上的最小代价和（BnB 下界） */
static double rows_min_bound(const double *cost, int n, int depth, const char *used)
{
    double s = 0.0;
    int r, c;
    for (r = depth; r < n; ++r) {
        double best = 1e300;
        for (c = 0; c < n; ++c)
            if (!used[c] && cost[r * n + c] < best) best = cost[r * n + c];
        if (best < 1e299) s += best;
    }
    return s;
}

static void assign_dfs(const double *cost, int n, int depth, char *used,
                       int *perm_cur, double cur,
                       double *best, int *perm_best)
{
    int c;
    if (depth == n) {
        if (cur < *best - 1e-12) {
            *best = cur;
            memcpy(perm_best, perm_cur, (size_t)n * sizeof(int));
        }
        return;
    }
    /* 剪枝：当前 + 未指派行最小和 仍无法改进 */
    if (cur + rows_min_bound(cost, n, depth, used) >= *best - 1e-12) return;
    for (c = 0; c < n; ++c) {
        if (used[c]) continue;
        used[c] = 1;
        perm_cur[depth] = c;
        assign_dfs(cost, n, depth + 1, used, perm_cur,
                   cur + cost[depth * n + c], best, perm_best);
        used[c] = 0;
    }
}

double mlab_assign_bnb(const double *cost, int n, int *perm_out)
{
    char *used = (char *)calloc((size_t)n, 1);
    int *perm_cur = (int *)malloc((size_t)n * sizeof(int));
    int *perm_best = (int *)malloc((size_t)n * sizeof(int));
    double best = 1e300;
    if (!used || !perm_cur || !perm_best || n <= 0 || n > 10) {
        free(used); free(perm_cur); free(perm_best);
        return 1e300;
    }
    assign_dfs(cost, n, 0, used, perm_cur, 0.0, &best, perm_best);
    if (perm_out) memcpy(perm_out, perm_best, (size_t)n * sizeof(int));
    free(used); free(perm_cur); free(perm_best);
    return best;
}

static void perm_dfs(const double *cost, int n, int depth, char *used,
                     int *perm_cur, double cur, double *best, int *perm_best)
{
    int c;
    if (depth == n) {
        if (cur < *best - 1e-12) {
            *best = cur;
            memcpy(perm_best, perm_cur, (size_t)n * sizeof(int));
        }
        return;
    }
    for (c = 0; c < n; ++c) {
        if (used[c]) continue;
        used[c] = 1;
        perm_cur[depth] = c;
        perm_dfs(cost, n, depth + 1, used, perm_cur,
                 cur + cost[depth * n + c], best, perm_best);
        used[c] = 0;
    }
}

double mlab_assign_enum(const double *cost, int n, int *perm_out)
{
    char *used = (char *)calloc((size_t)n, 1);
    int *perm_cur = (int *)malloc((size_t)n * sizeof(int));
    int *perm_best = (int *)malloc((size_t)n * sizeof(int));
    double best = 1e300;
    if (!used || !perm_cur || !perm_best || n <= 0 || n > 8) {
        free(used); free(perm_cur); free(perm_best);
        return 1e300;
    }
    perm_dfs(cost, n, 0, used, perm_cur, 0.0, &best, perm_best);
    if (perm_out) memcpy(perm_out, perm_best, (size_t)n * sizeof(int));
    free(used); free(perm_cur); free(perm_best);
    return best;
}
