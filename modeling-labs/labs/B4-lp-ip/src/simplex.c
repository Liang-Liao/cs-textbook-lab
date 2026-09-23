#include "simplex.h"
#include "linalg.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_simplex(const double *A, int m, int n, const double *b, const double *c,
                 double *x_opt, double *obj_out, double *y_out)
{
    /* Use two-phase tableau on slack/surplus for equality form via big-M free vars.
       For Ax=b, x>=0: start with basis from artificial variables (phase I). */
    int ntot = n + m; /* x + artificials */
    double *T = (double *)malloc((size_t)(m + 1) * (size_t)(ntot + 1) * sizeof(double));
    double *bnz = (double *)malloc((size_t)m * sizeof(double));
    int *basis = (int *)malloc((size_t)m * sizeof(int));
    int i, j, iter, rc = -1;
    const int max_iter = 2000;

    if (!T || !basis || !bnz) {
        free(T); free(basis); free(bnz);
        return -1;
    }
    /* b<0 行预处理：乘 -1 使人工变量可行基成立 */
    for (i = 0; i < m; ++i) bnz[i] = (b[i] < 0.0) ? -b[i] : b[i];
    /* tableau: rows 0..m-1 constraints, row m cost */
    memset(T, 0, (size_t)(m + 1) * (size_t)(ntot + 1) * sizeof(double));
    for (i = 0; i < m; ++i) {
        double sgn = (b[i] < 0.0) ? -1.0 : 1.0;
        for (j = 0; j < n; ++j) T[i * (ntot + 1) + j] = sgn * A[i * n + j];
        T[i * (ntot + 1) + (n + i)] = 1.0; /* artificial */
        T[i * (ntot + 1) + ntot] = bnz[i];
        basis[i] = n + i;
    }
    /* Phase I cost: sum artificials */
    for (j = 0; j < n; ++j) T[m * (ntot + 1) + j] = 0.0;
    for (j = 0; j < m; ++j) T[m * (ntot + 1) + (n + j)] = 1.0;
    T[m * (ntot + 1) + ntot] = 0.0;
    /* drive artificials out: make reduced costs correct by eliminating basis costs */
    for (i = 0; i < m; ++i) {
        double *row = T + i * (ntot + 1);
        double *cost = T + m * (ntot + 1);
        for (j = 0; j <= ntot; ++j) cost[j] -= row[j];
    }
    for (iter = 0; iter < max_iter; ++iter) {
        int enter = -1, leave = -1;
        double best_rc = -1e-12;
        double *cost = T + m * (ntot + 1);
        /* Bland: smallest index with negative reduced cost (min c_j) */
        for (j = 0; j < ntot; ++j) {
            if (cost[j] < -1e-10) {
                enter = j;
                break;
            }
        }
        (void)best_rc;
        if (enter < 0) break;
        {
            double best_ratio = 1e300;
            for (i = 0; i < m; ++i) {
                double aij = T[i * (ntot + 1) + enter];
                if (aij > 1e-10) {
                    double ratio = T[i * (ntot + 1) + ntot] / aij;
                    if (ratio < best_ratio - 1e-14 ||
                        (fabs(ratio - best_ratio) < 1e-14 && (leave < 0 || basis[i] < basis[leave]))) {
                        best_ratio = ratio;
                        leave = i;
                    }
                }
            }
        }
        if (leave < 0) {
            rc = 1; /* unbounded phase I — shouldn't happen if b>=0 */
            goto done;
        }
        /* pivot */
        {
            double piv = T[leave * (ntot + 1) + enter];
            for (j = 0; j <= ntot; ++j) T[leave * (ntot + 1) + j] /= piv;
            for (i = 0; i <= m; ++i) {
                if (i == leave) continue;
                double f = T[i * (ntot + 1) + enter];
                if (fabs(f) < 1e-300) continue;
                for (j = 0; j <= ntot; ++j)
                    T[i * (ntot + 1) + j] -= f * T[leave * (ntot + 1) + j];
            }
            basis[leave] = enter;
        }
    }
    /* Phase I objective = cost of artificials at row m, rhs */
    if (T[m * (ntot + 1) + ntot] < -1e-8 || T[m * (ntot + 1) + ntot] > 1e-8) {
        /* check sum of artificials */
        double art = 0.0;
        for (i = 0; i < m; ++i)
            if (basis[i] >= n) art += T[i * (ntot + 1) + ntot];
        if (art > 1e-7) {
            rc = -1; /* infeasible */
            goto done;
        }
    }
    /* Phase II: set cost to original c on x, 0 on artificials (force art out) */
    {
        double *cost = T + m * (ntot + 1);
        memset(cost, 0, (size_t)(ntot + 1) * sizeof(double));
        for (j = 0; j < n; ++j) cost[j] = c[j];
        cost[ntot] = 0.0;
        /* eliminate basic costs */
        for (i = 0; i < m; ++i) {
            double cb = cost[basis[i]];
            if (fabs(cb) < 1e-300) continue;
            for (j = 0; j <= ntot; ++j)
                cost[j] -= cb * T[i * (ntot + 1) + j];
        }
    }
    for (iter = 0; iter < max_iter; ++iter) {
        int enter = -1, leave = -1;
        double *cost = T + m * (ntot + 1);
        /* Phase II 只允许原始变量进基：人工列已用完其使命，
           若其 reduced cost 为负（=-y_i）被再次引入会破坏可行性 */
        for (j = 0; j < n; ++j) {
            if (cost[j] < -1e-10) {
                enter = j;
                break;
            }
        }
        if (enter < 0) break;
        {
            double best_ratio = 1e300;
            for (i = 0; i < m; ++i) {
                double aij = T[i * (ntot + 1) + enter];
                if (aij > 1e-10) {
                    double ratio = T[i * (ntot + 1) + ntot] / aij;
                    if (ratio < best_ratio - 1e-14 ||
                        (fabs(ratio - best_ratio) < 1e-14 && (leave < 0 || basis[i] < basis[leave]))) {
                        best_ratio = ratio;
                        leave = i;
                    }
                }
            }
        }
        if (leave < 0) {
            rc = 1;
            goto done;
        }
        {
            double piv = T[leave * (ntot + 1) + enter];
            for (j = 0; j <= ntot; ++j) T[leave * (ntot + 1) + j] /= piv;
            for (i = 0; i <= m; ++i) {
                if (i == leave) continue;
                double f = T[i * (ntot + 1) + enter];
                if (fabs(f) < 1e-300) continue;
                for (j = 0; j <= ntot; ++j)
                    T[i * (ntot + 1) + j] -= f * T[leave * (ntot + 1) + j];
            }
            basis[leave] = enter;
        }
    }
    /* extract x */
    memset(x_opt, 0, (size_t)n * sizeof(double));
    for (i = 0; i < m; ++i) {
        if (basis[i] < n)
            x_opt[basis[i]] = T[i * (ntot + 1) + ntot];
    }
    if (obj_out) {
        double s = 0.0;
        for (j = 0; j < n; ++j) s += c[j] * x_opt[j];
        *obj_out = s;
    }
    if (y_out) {
        /* duals: min c'x, Ax=b → y = c_B B^{-1}。
           人工列 n+i 的 reduced cost = 0 - y_i = -y_i，故取负号还原。 */
        for (i = 0; i < m; ++i)
            y_out[i] = -T[m * (ntot + 1) + (n + i)];
    }
    /* any artificial still basic at positive level => infeasible */
    for (i = 0; i < m; ++i)
        if (basis[i] >= n && T[i * (ntot + 1) + ntot] > 1e-7) {
            rc = -1;
            goto done;
        }
    rc = 0;
done:
    free(T); free(basis); free(bnz);
    return rc;
}
