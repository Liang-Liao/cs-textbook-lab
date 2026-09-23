#include "knapsack.h"
#include "linalg.h"
#include "vec.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_vertex_enum(const double *A, int m, int n, const double *b, const double *c,
                     double *x_opt, double *obj_out)
{
    /* choose m columns from n */
    double best = 1e300;
    int found = 0;
    int *idx = (int *)malloc((size_t)m * sizeof(int));
    double *Ab = (double *)malloc((size_t)m * (size_t)m * sizeof(double));
    double *sol = (double *)malloc((size_t)m * sizeof(double));
    int i, j, k;
    if (!idx || !Ab || !sol) {
        free(idx); free(Ab); free(sol);
        return -1;
    }
    /* recursive combination */
    {
        /* iterative odometer */
        for (i = 0; i < m; ++i) idx[i] = i;
        for (;;) {
            for (i = 0; i < m; ++i)
                for (j = 0; j < m; ++j)
                    Ab[i * m + j] = A[i * n + idx[j]];
            if (mlab_lu_solve_dense(Ab, m, b, sol) == 0) {
                int feas = 1;
                double obj = 0.0;
                for (i = 0; i < m; ++i)
                    if (sol[i] < -1e-8) feas = 0;
                if (feas) {
                    double *x = (double *)calloc((size_t)n, sizeof(double));
                    if (x) {
                        for (i = 0; i < m; ++i) x[idx[i]] = sol[i] < 0 ? 0 : sol[i];
                        for (j = 0; j < n; ++j) obj += c[j] * x[j];
                        if (obj < best) {
                            best = obj;
                            memcpy(x_opt, x, (size_t)n * sizeof(double));
                            found = 1;
                        }
                        free(x);
                    }
                }
            }
            /* next combination */
            k = m - 1;
            while (k >= 0 && idx[k] == n - m + k) --k;
            if (k < 0) break;
            ++idx[k];
            for (j = k + 1; j < m; ++j) idx[j] = idx[j - 1] + 1;
        }
    }
    if (obj_out && found) *obj_out = best;
    free(idx); free(Ab); free(sol);
    return found ? 0 : -1;
}

/* fractional knapsack value for bound */
static double frac_bound(const int *order, const double *w, const double *v,
                         int n, double cap, int from, const int *chosen)
{
    double rem = cap, val = 0.0;
    int i;
    (void)chosen;
    for (i = from; i < n; ++i) {
        int id = order[i];
        if (rem <= 0) break;
        if (w[id] <= rem) {
            rem -= w[id];
            val += v[id];
        } else {
            val += v[id] * (rem / w[id]);
            break;
        }
    }
    return val;
}

static void knapsack_dfs(const double *w, const double *v, const int *order,
                         int n, int depth, double cap, double cur_w, double cur_v,
                         int *xcur, double *best, int *xbest)
{
    if (depth == n) {
        if (cur_v > *best) {
            *best = cur_v;
            memcpy(xbest, xcur, (size_t)n * sizeof(int));
        }
        return;
    }
    {
        double ub = cur_v + frac_bound(order, w, v, n, cap - cur_w, depth, xcur);
        if (ub < *best - 1e-12) return;
    }
    {
        int id = order[depth];
        if (cur_w + w[id] <= cap + 1e-12) {
            xcur[id] = 1;
            knapsack_dfs(w, v, order, n, depth + 1, cap, cur_w + w[id], cur_v + v[id],
                         xcur, best, xbest);
            xcur[id] = 0;
        }
        knapsack_dfs(w, v, order, n, depth + 1, cap, cur_w, cur_v, xcur, best, xbest);
    }
}

static int cmp_ratio(const void *a, const void *b, const void *ctx)
{
    /* qsort_r not portable — use global-ish via casting order built outside */
    (void)a; (void)b; (void)ctx;
    return 0;
}

double mlab_knapsack_bb(const double *w, const double *v, int n, double cap,
                        int *x_sel)
{
    int *order = (int *)malloc((size_t)n * sizeof(int));
    int *xcur = (int *)calloc((size_t)n, sizeof(int));
    int *xbest = (int *)calloc((size_t)n, sizeof(int));
    double best = 0.0;
    int i, j;
    if (!order || !xcur || !xbest) {
        free(order); free(xcur); free(xbest);
        return 0.0;
    }
    for (i = 0; i < n; ++i) order[i] = i;
    /* sort by value/weight desc */
    for (i = 0; i < n; ++i)
        for (j = i + 1; j < n; ++j) {
            if (v[order[j]] / (w[order[j]] + 1e-300) > v[order[i]] / (w[order[i]] + 1e-300)) {
                int t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
        }
    knapsack_dfs(w, v, order, n, 0, cap, 0.0, 0.0, xcur, &best, xbest);
    if (x_sel) memcpy(x_sel, xbest, (size_t)n * sizeof(int));
    free(order); free(xcur); free(xbest);
    (void)cmp_ratio;
    return best;
}

double mlab_knapsack_brute(const double *w, const double *v, int n, double cap,
                           int *x_sel)
{
    double best = 0.0;
    unsigned long long mask, total;
    int *bestx = (int *)calloc((size_t)n, sizeof(int));
    if (!bestx) return 0.0;
    if (n > 62) return 0.0; /* 枚举上限守卫（1ULL<<n） */
    total = 1ULL << n;
    for (mask = 0; mask < total; ++mask) {
        double sw = 0, sv = 0;
        int i;
        for (i = 0; i < n; ++i)
            if (mask & (1ULL << i)) {
                sw += w[i];
                sv += v[i];
            }
        if (sw <= cap + 1e-12 && sv > best) {
            best = sv;
            for (i = 0; i < n; ++i) bestx[i] = (mask & (1ULL << i)) ? 1 : 0;
        }
    }
    if (x_sel) memcpy(x_sel, bestx, (size_t)n * sizeof(int));
    free(bestx);
    return best;
}
