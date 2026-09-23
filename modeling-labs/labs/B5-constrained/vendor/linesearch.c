#include "linesearch.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>

double mlab_golden_section(mlab_scalar_fn_1d f, void *ctx, double a, double b,
                           int max_iter, double tol, double *fmin_out,
                           double *widths_out)
{
    const double phi = 0.6180339887498949; /* (sqrt(5)-1)/2 */
    double x1, x2, f1, f2;
    int it;
    if (b <= a) {
        if (fmin_out) *fmin_out = f(a, ctx);
        return a;
    }
    x1 = b - phi * (b - a);
    x2 = a + phi * (b - a);
    f1 = f(x1, ctx);
    f2 = f(x2, ctx);
    for (it = 0; it < max_iter && (b - a) > tol; ++it) {
        if (f1 < f2) {
            b = x2;
            x2 = x1;
            f2 = f1;
            x1 = b - phi * (b - a);
            f1 = f(x1, ctx);
        } else {
            a = x1;
            x1 = x2;
            f1 = f2;
            x2 = a + phi * (b - a);
            f2 = f(x2, ctx);
        }
        if (widths_out) widths_out[it] = b - a;
    }
    {
        double xm = 0.5 * (a + b);
        double fm = f(xm, ctx);
        if (fmin_out) *fmin_out = fm;
        return xm;
    }
}

double mlab_armijo_backtrack(const mlab_objective *obj, const double *x,
                             const double *d, double alpha0,
                             double c1, int max_back, int *fevals_out)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    double slope, alpha, f0, ft;
    int k, fe = 0;

    if (!g || !xt || max_back <= 0 || !(alpha0 > 0.0)) {
        free(g);
        free(xt);
        if (fevals_out) *fevals_out = 0;
        return 0.0;
    }
    obj->grad(x, n, g, obj->ctx);
    slope = mlab_dot(g, d, n);
    f0 = obj->f(x, n, obj->ctx);
    ++fe;
    if (!(slope < 0.0)) {
        /* 非下降方向：Armijo 无意义（任何 alpha 都可能被平凡满足），拒绝 */
        free(g);
        free(xt);
        if (fevals_out) *fevals_out = fe;
        return 0.0;
    }
    alpha = alpha0;
    for (k = 0; k < max_back; ++k) {
        int i;
        for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * d[i];
        ft = obj->f(xt, n, obj->ctx);
        ++fe;
        if (ft <= f0 + c1 * alpha * slope) {
            free(g);
            free(xt);
            if (fevals_out) *fevals_out = fe;
            return alpha;
        }
        alpha *= 0.5;
    }
    free(g);
    free(xt);
    if (fevals_out) *fevals_out = fe;
    return 0.0; /* max_back 内无可接受步长 */
}

/* ---- strong Wolfe（bracket + zoom） ---- */

static double wl_f(const mlab_objective *obj, const double *x, const double *d,
                   int n, double alpha, double *xt, int *fe)
{
    int i;
    for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * d[i];
    ++(*fe);
    return obj->f(xt, n, obj->ctx);
}

static double wl_dphi(const mlab_objective *obj, const double *x, const double *d,
                      int n, double alpha, double *xt, double *gt, int *ge)
{
    int i;
    for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * d[i];
    obj->grad(xt, n, gt, obj->ctx);
    ++(*ge);
    return mlab_dot(gt, d, n);
}

/*
 * zoom(a_lo, a_hi)：假设 phi_lo = phi(a_lo) 满足 Armijo 且是当前最低点，
 * a_hi 在 a_lo 的"另一侧"夹住可接受区间。二分逼近，最多 100 次。
 */
static double wl_zoom(const mlab_objective *obj, const double *x, const double *d,
                      int n, double a_lo, double a_hi, double phi_lo,
                      double phi0, double dphi0, double c1, double c2,
                      double *xt, double *gt, int *fe, int *ge)
{
    int j;
    for (j = 0; j < 100; ++j) {
        double alpha = 0.5 * (a_lo + a_hi);
        double phi_j = wl_f(obj, x, d, n, alpha, xt, fe);
        if (phi_j > phi0 + c1 * alpha * dphi0 || phi_j >= phi_lo) {
            a_hi = alpha;
        } else {
            double dphi_j = wl_dphi(obj, x, d, n, alpha, xt, gt, ge);
            if (fabs(dphi_j) <= -c2 * dphi0) return alpha;
            if (dphi_j * (a_hi - a_lo) >= 0.0) a_hi = a_lo;
            a_lo = alpha;
            phi_lo = phi_j;
        }
        if (fabs(a_hi - a_lo) < 1e-16 * (1.0 + fabs(a_lo))) break;
    }
    return a_lo; /* 最后一轮的可行点（满足 Armijo） */
}

double mlab_wolfe_strong(const mlab_objective *obj, const double *x,
                         const double *d, double alpha0,
                         double c1, double c2, int max_iter,
                         int *fevals_out, int *gevals_out)
{
    int n = obj->dim;
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    double *gt = (double *)malloc((size_t)n * sizeof(double));
    double *g0 = (double *)malloc((size_t)n * sizeof(double));
    double phi0, dphi0, alpha_prev, phi_prev, alpha;
    int fe = 0, ge = 0, i;
    double result = 0.0;

    if (!xt || !gt || !g0 || max_iter <= 0 || !(c1 > 0.0) || !(c2 > c1) || !(c2 < 1.0))
        goto done;

    phi0 = obj->f(x, n, obj->ctx);
    ++fe;
    obj->grad(x, n, g0, obj->ctx);
    ++ge;
    dphi0 = mlab_dot(g0, d, n);
    if (!(dphi0 < 0.0)) goto done; /* 非下降方向 */

    alpha = alpha0 > 0.0 ? alpha0 : 1.0;
    alpha_prev = 0.0;
    phi_prev = phi0;
    for (i = 0; i < max_iter; ++i) {
        double phi_i = wl_f(obj, x, d, n, alpha, xt, &fe);
        if (phi_i > phi0 + c1 * alpha * dphi0 || (i > 0 && phi_i >= phi_prev)) {
            /* Armijo 失败或非单调 → 在 [alpha_prev, alpha] 内 zoom */
            result = wl_zoom(obj, x, d, n, alpha_prev, alpha, phi_prev,
                             phi0, dphi0, c1, c2, xt, gt, &fe, &ge);
            goto done;
        }
        {
            double dphi_i = wl_dphi(obj, x, d, n, alpha, xt, gt, &ge);
            if (fabs(dphi_i) <= -c2 * dphi0) {
                result = alpha; /* 两条 Wolfe 条件均满足 */
                goto done;
            }
            if (dphi_i >= 0.0) {
                /* 越过极小 → 在 [alpha, alpha_prev] 内 zoom */
                result = wl_zoom(obj, x, d, n, alpha, alpha_prev, phi_i,
                                 phi0, dphi0, c1, c2, xt, gt, &fe, &ge);
                goto done;
            }
        }
        alpha_prev = alpha;
        phi_prev = phi_i;
        alpha *= 2.0;
        if (!(alpha < 1e10)) break;
    }
done:
    free(xt);
    free(gt);
    free(g0);
    if (fevals_out) *fevals_out = fe;
    if (gevals_out) *gevals_out = ge;
    return result;
}

int mlab_steepest_descent_quad(const double *A, int n, double *x,
                               int max_iter, double tol,
                               double *res_hist, int *iter_out, int *fevals_out)
{
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *Ag = (double *)malloc((size_t)n * sizeof(double));
    int it, i, fe = 0;
    double e0;

    if (!g || !Ag) {
        free(g);
        free(Ag);
        return -1;
    }
    {
        double xtAx = 0.0;
        for (i = 0; i < n; ++i) {
            double s = 0.0;
            int j;
            for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
            xtAx += x[i] * s;
        }
        e0 = sqrt(xtAx > 0 ? xtAx : 1.0);
    }
    for (it = 0; it < max_iter; ++it) {
        double gTg, gAg, alpha, xtAx = 0.0;
        int j;
        for (i = 0; i < n; ++i) {
            double s = 0.0;
            for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
            xtAx += x[i] * s;
            g[i] = s;
        }
        ++fe;
        if (res_hist) res_hist[it] = sqrt(xtAx > 0 ? xtAx : 0.0) / e0;
        gTg = mlab_dot(g, g, n);
        if (gTg < tol * tol) {
            ++it;
            break;
        }
        for (i = 0; i < n; ++i) {
            double s = 0.0;
            for (j = 0; j < n; ++j) s += A[i * n + j] * g[j];
            Ag[i] = s;
        }
        gAg = mlab_dot(g, Ag, n);
        if (gAg <= 0) break;
        alpha = gTg / gAg;
        mlab_vec_axpy(x, -alpha, g, n);
    }
    if (iter_out) *iter_out = it;
    if (fevals_out) *fevals_out = fe;
    free(g);
    free(Ag);
    return 0;
}
