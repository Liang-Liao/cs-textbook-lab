#include "gp.h"
#include "linalg.h"
#include "dist.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_gp_init(mlab_gp *gp, int dim, const double *X, const double *y, int n,
                  double ls, double sf2, double noise)
{
    if (!gp) return;
    memset(gp, 0, sizeof *gp);
    gp->dim = dim;
    gp->n = n;
    gp->X = X;
    gp->y = y;
    gp->ls = ls > 1e-8 ? ls : 0.3;
    gp->sf2 = sf2 > 1e-12 ? sf2 : 1.0;
    gp->noise = noise > 1e-12 ? noise : 1e-4;
}

void mlab_gp_release(mlab_gp *gp)
{
    if (!gp) return;
    free(gp->L);
    free(gp->alpha);
    gp->L = NULL;
    gp->alpha = NULL;
    gp->fitted = 0;
}

double mlab_gp_rbf(const double *a, const double *b, int dim,
                   double ls, double sf2)
{
    double s = 0.0;
    int i;
    for (i = 0; i < dim; ++i) {
        double d = a[i] - b[i];
        s += d * d;
    }
    return sf2 * exp(-0.5 * s / (ls * ls));
}

int mlab_gp_fit(mlab_gp *gp)
{
    int n, i, j;
    double *K, *yc;
    if (!gp || !gp->X || !gp->y || gp->n < 2 || gp->dim < 1) return 0;
    n = gp->n;
    mlab_gp_release(gp);
    K = (double *)malloc((size_t)n * n * sizeof(double));
    yc = (double *)malloc((size_t)n * sizeof(double));
    gp->L = (double *)malloc((size_t)n * n * sizeof(double));
    gp->alpha = (double *)malloc((size_t)n * sizeof(double));
    if (!K || !yc || !gp->L || !gp->alpha) {
        free(K); free(yc);
        mlab_gp_release(gp);
        return 0;
    }
    gp->ymean = 0.0;
    for (i = 0; i < n; ++i) gp->ymean += gp->y[i];
    gp->ymean /= n;
    for (i = 0; i < n; ++i) yc[i] = gp->y[i] - gp->ymean;

    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j)
            K[i * n + j] = mlab_gp_rbf(&gp->X[i * gp->dim], &gp->X[j * gp->dim],
                                       gp->dim, gp->ls, gp->sf2);
        K[i * n + i] += gp->noise;
    }
    if (mlab_cholesky(K, n, gp->L) != 0) {
        /* 数值失败：加大噪声重试一次 */
        for (i = 0; i < n; ++i) K[i * n + i] += 1e-4;
        if (mlab_cholesky(K, n, gp->L) != 0) {
            free(K); free(yc);
            mlab_gp_release(gp);
            return 0;
        }
    }
    if (mlab_cholesky_solve(gp->L, n, yc, gp->alpha) != 0) {
        free(K); free(yc);
        mlab_gp_release(gp);
        return 0;
    }
    gp->fitted = 1;
    free(K);
    free(yc);
    return 1;
}

int mlab_gp_predict(const mlab_gp *gp, const double *x,
                    double *mean, double *var)
{
    int n, i, j;
    double *ks, *v, m, s;
    if (!gp || !gp->fitted || !x) return 0;
    n = gp->n;
    ks = (double *)malloc((size_t)n * sizeof(double));
    v = (double *)malloc((size_t)n * sizeof(double));
    if (!ks || !v) { free(ks); free(v); return 0; }
    for (i = 0; i < n; ++i)
        ks[i] = mlab_gp_rbf(&gp->X[i * gp->dim], x, gp->dim, gp->ls, gp->sf2);
    m = gp->ymean;
    for (i = 0; i < n; ++i) m += ks[i] * gp->alpha[i];
    /* v = L^{-1} ks */
    for (i = 0; i < n; ++i) {
        double acc = ks[i];
        for (j = 0; j < i; ++j) acc -= gp->L[i * n + j] * v[j];
        v[i] = acc / gp->L[i * n + i];
    }
    s = gp->sf2 + gp->noise; /* k(x,x) */
    for (i = 0; i < n; ++i) s -= v[i] * v[i];
    if (s < 1e-16) s = 1e-16;
    if (mean) *mean = m;
    if (var) *var = s;
    free(ks);
    free(v);
    return 1;
}

double mlab_acq_ei(double mean, double var, double best, double xi)
{
    double sd, g, phi, Phi;
    if (var <= 0.0) return 0.0;
    sd = sqrt(var);
    g = (best - mean - xi) / sd;
    phi = mlab_normal_pdf(g, 0.0, 1.0);
    Phi = mlab_normal_cdf(g, 0.0, 1.0);
    return (best - mean - xi) * Phi + sd * phi;
}

double mlab_acq_pi(double mean, double var, double best, double xi)
{
    double sd, g;
    if (var <= 0.0) return 0.0;
    sd = sqrt(var);
    g = (best - mean - xi) / sd;
    return mlab_normal_cdf(g, 0.0, 1.0);
}

double mlab_acq_ucb(double mean, double var, double kappa)
{
    return mean - kappa * (var > 0 ? sqrt(var) : 0.0);
}
