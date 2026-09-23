#include "box_qp.h"
#include "linalg.h"
#include "vec.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_box_qp_active_set(const double *Q, const double *c, const double *lb,
                           const double *ub, int n, double *x_opt, double *obj_out)
{
    unsigned long mask, total;
    double best = 1e300;
    int found = 0;
    if (n > 10) return -1;
    total = 1UL << n;
    for (mask = 0; mask < total; ++mask) {
        /* bit i set => variable i is FREE; else at bound (try both bounds) */
        int free_idx[10], nfree = 0;
        int bound_idx[10], nbound = 0;
        int bound_at_ub[10];
        int i, j, b;
        for (i = 0; i < n; ++i) {
            if (mask & (1UL << i)) free_idx[nfree++] = i;
            else bound_idx[nbound++] = i;
        }
        /* enumerate bound assignments: 2^nbound via bits */
        {
            unsigned long bmask, btot = 1UL << (nbound > 0 ? nbound : 0);
            if (nbound == 0) btot = 1;
            for (bmask = 0; bmask < btot; ++bmask) {
                double *x = (double *)malloc((size_t)n * sizeof(double));
                double *A, *rhs, *xf;
                int feas = 1;
                double obj = 0.0, *grad;
                if (!x) return -1;
                for (i = 0; i < nbound; ++i) {
                    int bi = bound_idx[i];
                    int use_ub = (bmask & (1UL << i)) ? 1 : 0;
                    bound_at_ub[i] = use_ub;
                    x[bi] = use_ub ? ub[bi] : lb[bi];
                }
                A = (double *)malloc((size_t)(nfree * nfree + 1) * sizeof(double));
                rhs = (double *)malloc((size_t)(nfree + 1) * sizeof(double));
                xf = (double *)malloc((size_t)(nfree + 1) * sizeof(double));
                grad = (double *)malloc((size_t)n * sizeof(double));
                if (!A || !rhs || !xf || !grad) {
                    free(x); free(A); free(rhs); free(xf); free(grad);
                    return -1;
                }
                if (nfree > 0) {
                    for (i = 0; i < nfree; ++i) {
                        for (j = 0; j < nfree; ++j)
                            A[i * nfree + j] = Q[free_idx[i] * n + free_idx[j]];
                        rhs[i] = -c[free_idx[i]];
                        for (j = 0; j < nbound; ++j)
                            rhs[i] -= Q[free_idx[i] * n + bound_idx[j]] * x[bound_idx[j]];
                    }
                    if (mlab_lu_solve_dense(A, nfree, rhs, xf) != 0)
                        feas = 0;
                    else {
                        for (i = 0; i < nfree; ++i) {
                            x[free_idx[i]] = xf[i];
                            if (xf[i] < lb[free_idx[i]] - 1e-8 ||
                                xf[i] > ub[free_idx[i]] + 1e-8)
                                feas = 0;
                        }
                    }
                }
                /* check reduced gradient signs on bounds */
                for (i = 0; i < n; ++i) {
                    double s = c[i];
                    for (j = 0; j < n; ++j) s += Q[i * n + j] * x[j];
                    grad[i] = s;
                }
                for (i = 0; i < nbound; ++i) {
                    int bi = bound_idx[i];
                    if (bound_at_ub[i]) {
                        if (grad[bi] > 1e-6) feas = 0; /* need grad<=0 at ub for min */
                    } else {
                        if (grad[bi] < -1e-6) feas = 0; /* need grad>=0 at lb */
                    }
                }
                for (i = 0; i < nfree; ++i)
                    if (fabs(grad[free_idx[i]]) > 1e-5) feas = 0;
                if (feas) {
                    for (i = 0; i < n; ++i) {
                        obj += c[i] * x[i];
                        for (j = 0; j < n; ++j) obj += 0.5 * x[i] * Q[i * n + j] * x[j];
                    }
                    if (obj < best - 1e-12) {
                        best = obj;
                        memcpy(x_opt, x, (size_t)n * sizeof(double));
                        found = 1;
                    } else if (!found || obj < best) {
                        if (obj < best) {
                            best = obj;
                            memcpy(x_opt, x, (size_t)n * sizeof(double));
                            found = 1;
                        }
                    }
                }
                free(x); free(A); free(rhs); free(xf); free(grad);
            }
        }
    }
    if (obj_out && found) *obj_out = best;
    return found ? 0 : -1;
}
