#include "interp.h"
#include "linalg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Lagrange / Newton ---------- */

double mlab_lagrange_eval(const double *xn, const double *yn, int n, double x)
{
    double sum = 0.0;
    int j, m;
    if (n <= 0) return 0.0;
    for (j = 0; j < n; ++j) {
        double num = 1.0, den = 1.0, term;
        for (m = 0; m < n; ++m) {
            if (m == j) continue;
            num *= (x - xn[m]);
            den *= (xn[j] - xn[m]);
        }
        term = (fabs(den) < 1e-300) ? 0.0 : yn[j] * num / den;
        sum += term;
    }
    return sum;
}

int mlab_newton_divided_diff(const double *xn, const double *yn, int n, double *coef)
{
    int i, j;
    if (n <= 0 || !xn || !yn || !coef) return -1;
    for (i = 0; i < n; ++i) coef[i] = yn[i];
    for (j = 1; j < n; ++j) {
        for (i = n - 1; i >= j; --i) {
            double dx = xn[i] - xn[i - j];
            if (fabs(dx) < 1e-300) return -1;
            coef[i] = (coef[i] - coef[i - 1]) / dx;
        }
    }
    return 0;
}

double mlab_newton_eval(const double *xn, const double *coef, int n, double x)
{
    double acc;
    int i;
    if (n <= 0) return 0.0;
    acc = coef[n - 1];
    for (i = n - 2; i >= 0; --i)
        acc = acc * (x - xn[i]) + coef[i];
    return acc;
}

/* ---------- piecewise / Hermite ---------- */

static int locate_interval(const double *xn, int n, double xq)
{
    int lo = 0, hi = n - 2, mid;
    if (n < 2) return 0;
    if (xq <= xn[0]) return 0;
    if (xq >= xn[n - 1]) return n - 2;
    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        if (xn[mid] <= xq) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}

double mlab_piecewise_linear_eval(const double *xn, const double *yn, int n, double xq)
{
    int i;
    double h, t;
    if (n <= 0) return 0.0;
    if (n == 1) return yn[0];
    if (xq <= xn[0]) return yn[0];
    if (xq >= xn[n - 1]) return yn[n - 1];
    i = locate_interval(xn, n, xq);
    h = xn[i + 1] - xn[i];
    if (fabs(h) < 1e-300) return yn[i];
    t = (xq - xn[i]) / h;
    return (1.0 - t) * yn[i] + t * yn[i + 1];
}

double mlab_cubic_hermite_eval(const double *xn, const double *yn, const double *yp,
                               int n, double xq)
{
    int i;
    double h, t, t2, t3, h00, h10, h01, h11;
    if (n <= 0) return 0.0;
    if (n == 1) return yn[0];
    if (xq <= xn[0]) return yn[0];
    if (xq >= xn[n - 1]) return yn[n - 1];
    i = locate_interval(xn, n, xq);
    h = xn[i + 1] - xn[i];
    if (fabs(h) < 1e-300) return yn[i];
    t = (xq - xn[i]) / h;
    t2 = t * t;
    t3 = t2 * t;
    h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    h10 = t3 - 2.0 * t2 + t;
    h01 = -2.0 * t3 + 3.0 * t2;
    h11 = t3 - t2;
    return h00 * yn[i] + h10 * h * yp[i] + h01 * yn[i + 1] + h11 * h * yp[i + 1];
}

/* ---------- Thomas + natural cubic spline ---------- */

int mlab_thomas(int n, const double *a, const double *b, const double *c,
                const double *d, double *x)
{
    int i;
    double *bb, *dd;
    if (n <= 0 || !a || !b || !c || !d || !x) return -1;
    bb = (double *)malloc((size_t)n * sizeof(double));
    dd = (double *)malloc((size_t)n * sizeof(double));
    if (!bb || !dd) {
        free(bb);
        free(dd);
        return -1;
    }
    memcpy(bb, b, (size_t)n * sizeof(double));
    memcpy(dd, d, (size_t)n * sizeof(double));
    for (i = 1; i < n; ++i) {
        double m;
        if (fabs(bb[i - 1]) < 1e-300) {
            free(bb);
            free(dd);
            return -1;
        }
        m = a[i] / bb[i - 1];
        bb[i] -= m * c[i - 1];
        dd[i] -= m * dd[i - 1];
    }
    if (fabs(bb[n - 1]) < 1e-300) {
        free(bb);
        free(dd);
        return -1;
    }
    x[n - 1] = dd[n - 1] / bb[n - 1];
    for (i = n - 2; i >= 0; --i)
        x[i] = (dd[i] - c[i] * x[i + 1]) / bb[i];
    free(bb);
    free(dd);
    return 0;
}

void mlab_cspline_free(mlab_cspline *sp)
{
    if (!sp) return;
    free(sp->x);
    free(sp->y);
    free(sp->M);
    sp->x = sp->y = sp->M = NULL;
    sp->n = 0;
}

int mlab_cspline_fit(mlab_cspline *sp, const double *xn, const double *yn, int n)
{
    int i, m, rc;
    double *h = NULL, *sub = NULL, *dia = NULL, *sup = NULL, *rhs = NULL, *Mi = NULL;

    if (!sp || !xn || !yn || n < 2) return -1;
    memset(sp, 0, sizeof *sp);
    sp->x = (double *)malloc((size_t)n * sizeof(double));
    sp->y = (double *)malloc((size_t)n * sizeof(double));
    sp->M = (double *)calloc((size_t)n, sizeof(double));
    if (!sp->x || !sp->y || !sp->M) {
        mlab_cspline_free(sp);
        return -1;
    }
    memcpy(sp->x, xn, (size_t)n * sizeof(double));
    memcpy(sp->y, yn, (size_t)n * sizeof(double));
    sp->n = n;

    if (n == 2) {
        /* 线性：M=0 */
        return 0;
    }

    h = (double *)malloc((size_t)(n - 1) * sizeof(double));
    if (!h) {
        mlab_cspline_free(sp);
        return -1;
    }
    for (i = 0; i < n - 1; ++i) {
        h[i] = xn[i + 1] - xn[i];
        if (h[i] <= 0.0) {
            free(h);
            mlab_cspline_free(sp);
            return -1;
        }
    }

    m = n - 2; /* 未知 M_1..M_{n-2} */
    sub = (double *)calloc((size_t)m, sizeof(double));
    dia = (double *)calloc((size_t)m, sizeof(double));
    sup = (double *)calloc((size_t)m, sizeof(double));
    rhs = (double *)calloc((size_t)m, sizeof(double));
    Mi = (double *)calloc((size_t)m, sizeof(double));
    if (!sub || !dia || !sup || !rhs || !Mi) {
        free(h);
        free(sub);
        free(dia);
        free(sup);
        free(rhs);
        free(Mi);
        mlab_cspline_free(sp);
        return -1;
    }

    for (i = 0; i < m; ++i) {
        /* 节点 j = i+1 */
        int j = i + 1;
        double left = (yn[j] - yn[j - 1]) / h[j - 1];
        double right = (yn[j + 1] - yn[j]) / h[j];
        dia[i] = 2.0 * (h[j - 1] + h[j]);
        if (i > 0) sub[i] = h[j - 1];
        if (i < m - 1) sup[i] = h[j];
        rhs[i] = 6.0 * (right - left);
    }
    rc = mlab_thomas(m, sub, dia, sup, rhs, Mi);
    if (rc == 0) {
        sp->M[0] = 0.0;
        sp->M[n - 1] = 0.0;
        for (i = 0; i < m; ++i) sp->M[i + 1] = Mi[i];
    }
    free(h);
    free(sub);
    free(dia);
    free(sup);
    free(rhs);
    free(Mi);
    return rc;
}

double mlab_cspline_eval(const mlab_cspline *sp, double xq)
{
    int i;
    double hi, dx1, dx2, si;
    if (!sp || sp->n < 2) return 0.0;
    if (xq <= sp->x[0]) return sp->y[0];
    if (xq >= sp->x[sp->n - 1]) return sp->y[sp->n - 1];
    i = locate_interval(sp->x, sp->n, xq);
    hi = sp->x[i + 1] - sp->x[i];
    if (fabs(hi) < 1e-300) return sp->y[i];
    dx1 = sp->x[i + 1] - xq;
    dx2 = xq - sp->x[i];
    si = (sp->M[i] / (6.0 * hi)) * dx1 * dx1 * dx1
       + (sp->M[i + 1] / (6.0 * hi)) * dx2 * dx2 * dx2
       + (sp->y[i] / hi - sp->M[i] * hi / 6.0) * dx1
       + (sp->y[i + 1] / hi - sp->M[i + 1] * hi / 6.0) * dx2;
    return si;
}

/* ---------- RBF ---------- */

double mlab_rbf_kernel(int kind, double shape, double r)
{
    if (kind == MLAB_RBF_GAUSSIAN) {
        double er = shape * r;
        return exp(-er * er);
    }
    /* thin-plate spline: r^2 log r */
    if (r < 1e-300) return 0.0;
    return r * r * log(r);
}

static double dist_eu(const double *a, const double *b, int dim)
{
    double s = 0.0;
    int d;
    for (d = 0; d < dim; ++d) {
        double t = a[d] - b[d];
        s += t * t;
    }
    return sqrt(s);
}

int mlab_rbf_fit(mlab_rbf *rbf, const double *xs, const double *ys,
                 int n, int dim, int kind, double shape)
{
    int i, j, rc;
    double extent = 0.0;
    if (!rbf || !xs || !ys || n <= 0 || dim <= 0) return -1;
    /* 重复节点早拒：TPS 核矩阵奇异、Gaussian 也使解退化 */
    for (i = 0; i < n; ++i) {
        int d;
        for (d = 0; d < dim; ++d) {
            if (xs[i * dim + d] > extent) extent = xs[i * dim + d];
            if (-xs[i * dim + d] > extent) extent = -xs[i * dim + d];
        }
    }
    extent = fmax(extent, 1.0);
    for (i = 0; i < n; ++i) {
        for (j = i + 1; j < n; ++j) {
            double r = dist_eu(&xs[(size_t)i * dim], &xs[(size_t)j * dim], dim);
            if (r < 1e-9 * extent) return -2;
        }
    }
    memset(rbf, 0, sizeof *rbf);
    rbf->n = n;
    rbf->dim = dim;
    rbf->kind = kind;
    rbf->shape = shape;
    rbf->xs = (double *)malloc((size_t)n * (size_t)dim * sizeof(double));
    rbf->ys = (double *)malloc((size_t)n * sizeof(double));
    rbf->alpha = (double *)malloc((size_t)n * sizeof(double));
    rbf->A = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!rbf->xs || !rbf->ys || !rbf->alpha || !rbf->A) {
        mlab_rbf_free(rbf);
        return -1;
    }
    memcpy(rbf->xs, xs, (size_t)n * (size_t)dim * sizeof(double));
    memcpy(rbf->ys, ys, (size_t)n * sizeof(double));
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double r = dist_eu(&xs[(size_t)i * dim], &xs[(size_t)j * dim], dim);
            rbf->A[(size_t)i * n + j] = mlab_rbf_kernel(kind, shape, r);
        }
        rbf->alpha[i] = ys[i];
    }
    if (kind == MLAB_RBF_TPS) {
        /* 鞍点系统 [[A, P],[P^T, 0]]·[α; pc] = [y; 0]，P = [1, x] */
        int m = dim + 1, nm = n + m;
        double *S = (double *)calloc((size_t)nm * (size_t)nm, sizeof(double));
        double *rhs = (double *)calloc((size_t)nm, sizeof(double));
        double *sol = (double *)malloc((size_t)nm * sizeof(double));
        if (!S || !rhs || !sol) {
            free(S); free(rhs); free(sol);
            mlab_rbf_free(rbf);
            return -1;
        }
        for (i = 0; i < n; ++i) {
            int d;
            for (j = 0; j < n; ++j)
                S[i * nm + j] = rbf->A[i * n + j];
            S[i * nm + n] = 1.0;          /* P[i][0] = 1 */
            for (d = 0; d < dim; ++d)
                S[i * nm + n + 1 + d] = xs[i * dim + d];
            rhs[i] = ys[i];
        }
        for (j = 0; j < m; ++j)
            for (i = 0; i < n; ++i)
                S[(n + j) * nm + i] = (j == 0) ? 1.0 : xs[i * dim + (j - 1)];
        rc = mlab_lu_solve_dense(S, nm, rhs, sol);
        free(S);
        free(rhs);
        if (rc != 0) {
            free(sol);
            mlab_rbf_free(rbf);
            return -1;
        }
        for (i = 0; i < n; ++i) rbf->alpha[i] = sol[i];
        rbf->poly = 1;
        rbf->m = m;
        rbf->pc = (double *)malloc((size_t)m * sizeof(double));
        if (!rbf->pc) {
            free(sol);
            mlab_rbf_free(rbf);
            return -1;
        }
        for (j = 0; j < m; ++j) rbf->pc[j] = sol[n + j];
        free(sol);
        return 0;
    }
    rc = mlab_lu_solve_dense(rbf->A, n, rbf->ys, rbf->alpha);
    return rc;
}

double mlab_rbf_eval(const mlab_rbf *rbf, const double *x)
{
    double s = 0.0;
    int j;
    if (!rbf || !rbf->alpha) return 0.0;
    for (j = 0; j < rbf->n; ++j) {
        double r = dist_eu(x, &rbf->xs[(size_t)j * rbf->dim], rbf->dim);
        s += rbf->alpha[j] * mlab_rbf_kernel(rbf->kind, rbf->shape, r);
    }
    if (rbf->poly && rbf->pc) {
        int d;
        s += rbf->pc[0];
        for (d = 0; d < rbf->dim; ++d)
            s += rbf->pc[1 + d] * x[d];
    }
    return s;
}

double mlab_rbf_cond2(const mlab_rbf *rbf)
{
    if (!rbf || !rbf->A || rbf->n <= 0) return -1.0;
    return mlab_cond2_estimate(rbf->A, rbf->n);
}

void mlab_rbf_free(mlab_rbf *rbf)
{
    if (!rbf) return;
    free(rbf->A);
    free(rbf->alpha);
    free(rbf->xs);
    free(rbf->ys);
    free(rbf->pc);
    rbf->A = rbf->alpha = rbf->xs = rbf->ys = rbf->pc = NULL;
    rbf->n = 0;
    rbf->poly = 0;
    rbf->m = 0;
}

/* ---------- 1D kriging（与 C9 GP 同源） ---------- */

int mlab_kriging1d_fit(mlab_kriging1d *k, const double *x, const double *y,
                       int n, double ls, double sf2, double nugget)
{
    int i, j, rc;
    double *K;
    if (!k || !x || !y || n <= 0) return -1;
    memset(k, 0, sizeof *k);
    k->n = n;
    k->ls = ls;
    k->sf2 = sf2;
    k->nugget = nugget;
    k->x = (double *)malloc((size_t)n * sizeof(double));
    k->w = (double *)malloc((size_t)n * sizeof(double));
    K = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!k->x || !k->w || !K) {
        free(K);
        mlab_kriging1d_free(k);
        return -1;
    }
    memcpy(k->x, x, (size_t)n * sizeof(double));
    k->ymean = 0.0;
    for (i = 0; i < n; ++i) k->ymean += y[i];
    k->ymean /= (double)n;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) {
            double d = x[i] - x[j];
            K[i * n + j] = sf2 * exp(-0.5 * d * d / (ls * ls));
            if (i == j) K[i * n + i] += nugget;
        }
    for (i = 0; i < n; ++i) k->w[i] = y[i] - k->ymean;
    rc = mlab_lu_solve_dense(K, n, k->w, k->w);
    free(K);
    if (rc != 0) {
        mlab_kriging1d_free(k);
        return -1;
    }
    return 0;
}

double mlab_kriging1d_eval(const mlab_kriging1d *k, double xq)
{
    double s;
    int i;
    if (!k || !k->w || k->n <= 0) return 0.0;
    s = k->ymean;
    for (i = 0; i < k->n; ++i) {
        double d = xq - k->x[i];
        s += k->w[i] * k->sf2 * exp(-0.5 * d * d / (k->ls * k->ls));
    }
    return s;
}

void mlab_kriging1d_free(mlab_kriging1d *k)
{
    if (!k) return;
    free(k->x);
    free(k->w);
    k->x = k->w = NULL;
    k->n = 0;
}

/* ---------- PGM ---------- */

void mlab_write_pgm_grid(const char *path, const double *val, int ncol, int nrow)
{
    FILE *fp;
    int r, c;
    double vmin, vmax, span;
    if (!path || !val || ncol <= 0 || nrow <= 0) return;
    fp = fopen(path, "w");
    if (!fp) return;
    vmin = vmax = val[0];
    for (r = 0; r < nrow * ncol; ++r) {
        if (val[r] < vmin) vmin = val[r];
        if (val[r] > vmax) vmax = val[r];
    }
    span = vmax - vmin;
    if (!(span > 0.0)) span = 1.0;
    fprintf(fp, "P2\n%d %d\n255\n", ncol, nrow);
    for (r = 0; r < nrow; ++r) {
        for (c = 0; c < ncol; ++c) {
            double t = (val[r * ncol + c] - vmin) / span;
            int v = (int)(t * 255.0 + 0.5);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            fprintf(fp, "%d%c", v, c + 1 == ncol ? '\n' : ' ');
        }
    }
    fclose(fp);
}
