#include "lsq.h"
#include "linalg.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_lsq_normal(const double *X, int m, int n, const double *y, double *beta)
{
    double *XtX = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *Xty = (double *)malloc((size_t)n * sizeof(double));
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    int i, j, k, rc;
    if (!XtX || !Xty || !L) {
        free(XtX); free(Xty); free(L);
        return -1;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (k = 0; k < m; ++k) s += X[k * n + i] * X[k * n + j];
            XtX[i * n + j] = s;
        }
        {
            double s = 0.0;
            for (k = 0; k < m; ++k) s += X[k * n + i] * y[k];
            Xty[i] = s;
        }
    }
    rc = mlab_cholesky(XtX, n, L);
    if (rc == 0) rc = mlab_cholesky_solve(L, n, Xty, beta);
    free(XtX); free(Xty); free(L);
    return rc;
}

int mlab_lsq_qr(const double *X, int m, int n, const double *y, double *beta)
{
    /* MGS QR: X = Q R, beta = R^{-1} Q^T y */
    double *Q = (double *)malloc((size_t)m * (size_t)n * sizeof(double));
    double *R = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *Qty = (double *)malloc((size_t)n * sizeof(double));
    double scale_thresh = 0.0;
    int i, j, k;
    if (!Q || !R || !Qty) {
        free(Q); free(R); free(Qty);
        return -1;
    }
    memcpy(Q, X, (size_t)m * (size_t)n * sizeof(double));
    memset(R, 0, (size_t)n * (size_t)n * sizeof(double));
    {
        /* 矩阵尺度（最大绝对元），用于近秩亏的相对阈值 */
        double scale = 0.0;
        size_t it;
        for (it = 0; it < (size_t)m * n; ++it)
            if (fabs(X[it]) > scale) scale = fabs(X[it]);
        if (scale <= 0.0) scale = 1.0;
        scale_thresh = 1e-12 * scale;
    }
    for (j = 0; j < n; ++j) {
        for (k = 0; k < j; ++k) {
            double d = 0.0;
            for (i = 0; i < m; ++i) d += Q[i * n + k] * Q[i * n + j];
            R[k * n + j] = d;
            for (i = 0; i < m; ++i) Q[i * n + j] -= d * Q[i * n + k];
        }
        {
            double nrm = 0.0;
            for (i = 0; i < m; ++i) nrm += Q[i * n + j] * Q[i * n + j];
            nrm = sqrt(nrm);
            if (nrm < scale_thresh) {
                /* 近秩亏：与分配失败区分的分级返回 */
                free(Q); free(R); free(Qty);
                return -2;
            }
            R[j * n + j] = nrm;
            for (i = 0; i < m; ++i) Q[i * n + j] /= nrm;
        }
    }
    for (j = 0; j < n; ++j) {
        double s = 0.0;
        for (i = 0; i < m; ++i) s += Q[i * n + j] * y[i];
        Qty[j] = s;
    }
    /* back-sub R beta = Qty */
    for (j = n - 1; j >= 0; --j) {
        double s = Qty[j];
        for (k = j + 1; k < n; ++k) s -= R[j * n + k] * beta[k];
        beta[j] = s / R[j * n + j];
    }
    free(Q); free(R); free(Qty);
    return 0;
}

double mlab_lsq_rss(const double *X, int m, int n, const double *y, const double *beta)
{
    double s = 0.0;
    int i, j;
    for (i = 0; i < m; ++i) {
        double yh = 0.0;
        for (j = 0; j < n; ++j) yh += X[i * n + j] * beta[j];
        s += (y[i] - yh) * (y[i] - yh);
    }
    return s;
}

int mlab_lsq_cov(const double *X, int m, int n, double sigma2, double *cov_out)
{
    double *XtX = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *inv = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *e = (double *)malloc((size_t)n * sizeof(double));
    double *col = (double *)malloc((size_t)n * sizeof(double));
    int i, j, k;
    if (!XtX || !inv || !L || !e || !col) {
        free(XtX); free(inv); free(L); free(e); free(col);
        return -1;
    }
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (k = 0; k < m; ++k) s += X[k * n + i] * X[k * n + j];
            XtX[i * n + j] = s;
        }
    if (mlab_cholesky(XtX, n, L) != 0) {
        free(XtX); free(inv); free(L); free(e); free(col);
        return -1;
    }
    for (j = 0; j < n; ++j) {
        for (i = 0; i < n; ++i) e[i] = 0.0;
        e[j] = 1.0;
        mlab_cholesky_solve(L, n, e, col);
        for (i = 0; i < n; ++i) inv[i * n + j] = col[i];
    }
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            cov_out[i * n + j] = sigma2 * inv[i * n + j];
    free(XtX); free(inv); free(L); free(e); free(col);
    return 0;
}

int mlab_lm(const double *t, const double *y, int m,
            mlab_nl_model model, void *ctx,
            double *theta, int npar, int max_iter, double tol, int *iters_out)
{
    const double h = 1e-6;
    double *J = (double *)malloc((size_t)m * (size_t)npar * sizeof(double));
    double *r = (double *)malloc((size_t)m * sizeof(double));
    double *JtJ = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
    double *Jtr = (double *)malloc((size_t)npar * sizeof(double));
    double *A = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
    double *g = (double *)malloc((size_t)npar * sizeof(double));
    double *L = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
    double *dth = (double *)malloc((size_t)npar * sizeof(double));
    double *th_try = (double *)malloc((size_t)npar * sizeof(double));
    double lambda = 1e-3;
    int it, i, j, k;

    if (!J || !r || !JtJ || !Jtr || !A || !g || !L || !dth || !th_try) {
        free(J); free(r); free(JtJ); free(Jtr); free(A); free(g);
        free(L); free(dth); free(th_try);
        return -1;
    }
    for (it = 0; it < max_iter; ++it) {
        double rss = 0.0, rss_try;
        for (i = 0; i < m; ++i) {
            r[i] = model(t + i, theta, npar, ctx) - y[i];
            rss += r[i] * r[i];
        }
        /* Jacobian FD */
        for (j = 0; j < npar; ++j) {
            double save = theta[j];
            double hj = h * (fabs(save) > 1.0 ? fabs(save) : 1.0);
            theta[j] = save + hj;
            for (i = 0; i < m; ++i)
                J[i * npar + j] = (model(t + i, theta, npar, ctx) - (r[i] + y[i])) / hj;
            theta[j] = save;
        }
        /* JtJ, Jtr */
        for (j = 0; j < npar; ++j) {
            for (k = 0; k < npar; ++k) {
                double s = 0.0;
                for (i = 0; i < m; ++i) s += J[i * npar + j] * J[i * npar + k];
                JtJ[j * npar + k] = s;
            }
            {
                double s = 0.0;
                for (i = 0; i < m; ++i) s += J[i * npar + j] * r[i];
                Jtr[j] = s;
            }
        }
        /* (JtJ + λ diag) dth = -Jtr */
        {
            int inner, ok = 0;
            for (inner = 0; inner < 20 && !ok; ++inner) {
                for (j = 0; j < npar; ++j) {
                    for (k = 0; k < npar; ++k)
                        A[j * npar + k] = JtJ[j * npar + k] + ((j == k) ? lambda * (JtJ[j * npar + j] + 1e-12) : 0.0);
                    g[j] = -Jtr[j];
                }
                if (mlab_cholesky(A, npar, L) == 0 &&
                    mlab_cholesky_solve(L, npar, g, dth) == 0) {
                    for (j = 0; j < npar; ++j) th_try[j] = theta[j] + dth[j];
                    rss_try = 0.0;
                    for (i = 0; i < m; ++i) {
                        double rr = model(t + i, th_try, npar, ctx) - y[i];
                        rss_try += rr * rr;
                    }
                    if (rss_try < rss) {
                        memcpy(theta, th_try, (size_t)npar * sizeof(double));
                        lambda *= 0.3;
                        if (lambda < 1e-12) lambda = 1e-12;
                        ok = 1;
                        if (fabs(rss - rss_try) < tol * (1.0 + rss)) {
                            if (iters_out) *iters_out = it + 1;
                            free(J); free(r); free(JtJ); free(Jtr); free(A);
                            free(g); free(L); free(dth); free(th_try);
                            return 0;
                        }
                    } else {
                        lambda *= 5.0;
                        if (lambda > 1e8) lambda = 1e8;
                    }
                } else {
                    lambda *= 10.0;
                }
            }
            if (!ok) {
                if (iters_out) *iters_out = it;
                free(J); free(r); free(JtJ); free(Jtr); free(A);
                free(g); free(L); free(dth); free(th_try);
                return 2; /* 内层停滞：θ 保持当前值，调用方可区分 */
            }
        }
    }
    if (iters_out) *iters_out = max_iter;
    free(J); free(r); free(JtJ); free(Jtr); free(A);
    free(g); free(L); free(dth); free(th_try);
    return 1; /* max_iter 用尽未触发 tol 收敛 */
}

double mlab_lsq_r2(const double *X, int m, int n, const double *y, const double *beta)
{
    double ybar = 0.0, tss = 0.0, rss = 0.0;
    int i, j;
    if (m <= 0) return 0.0;
    for (i = 0; i < m; ++i) ybar += y[i];
    ybar /= m;
    for (i = 0; i < m; ++i) {
        double yh = 0.0, d;
        for (j = 0; j < n; ++j) yh += X[i * n + j] * beta[j];
        d = y[i] - ybar;
        tss += d * d;
        d = y[i] - yh;
        rss += d * d;
    }
    return (tss > 0.0) ? 1.0 - rss / tss : 0.0;
}

int mlab_lsq_weighted(const double *X, int m, int n, const double *y,
                      const double *w, double *beta)
{
    /* 行缩放：Xw[i][j] = sqrt(w_i) X[i][j]，yw[i] = sqrt(w_i) y[i] */
    double *Xw = (double *)malloc((size_t)m * n * sizeof(double));
    double *yw = (double *)malloc((size_t)m * sizeof(double));
    int i, j, rc = 0;
    if (!Xw || !yw) {
        free(Xw); free(yw);
        return -1;
    }
    for (i = 0; i < m; ++i) {
        double sw;
        if (!(w[i] > 0.0)) {
            free(Xw); free(yw);
            return -1;
        }
        sw = sqrt(w[i]);
        for (j = 0; j < n; ++j) Xw[i * n + j] = sw * X[i * n + j];
        yw[i] = sw * y[i];
    }
    rc = mlab_lsq_normal(Xw, m, n, yw, beta);
    free(Xw);
    free(yw);
    return rc;
}

int mlab_lsq_huber(const double *X, int m, int n, const double *y,
                   double delta, int max_iter, double *beta, int *iters_out)
{
    double *r = (double *)malloc((size_t)m * sizeof(double));
    double *w = (double *)malloc((size_t)m * sizeof(double));
    double *beta_old = (double *)malloc((size_t)n * sizeof(double));
    int it, i, j, ok = -1;
    if (!r || !w || !beta_old || !(delta > 0.0)) {
        free(r); free(w); free(beta_old);
        return -1;
    }
    /* 初始解：普通 LS */
    if (mlab_lsq_normal(X, m, n, y, beta) != 0) {
        free(r); free(w); free(beta_old);
        return -1;
    }
    for (it = 0; it < max_iter; ++it) {
        double diff = 0.0;
        /* 残差与 Huber 权重：|r|<=δ → 1；否则 δ/|r| */
        for (i = 0; i < m; ++i) {
            double yh = 0.0;
            for (j = 0; j < n; ++j) yh += X[i * n + j] * beta[j];
            r[i] = y[i] - yh;
            w[i] = (fabs(r[i]) <= delta) ? 1.0 : delta / fabs(r[i]);
        }
        mlab_vec_copy(beta_old, beta, n);
        if (mlab_lsq_weighted(X, m, n, y, w, beta) != 0) break;
        for (j = 0; j < n; ++j)
            diff += fabs(beta[j] - beta_old[j]);
        if (diff < 1e-10 * (1.0 + mlab_nrm2(beta, n))) {
            ok = 0;
            it = it + 1;
            break;
        }
        ok = 0;
    }
    if (iters_out) *iters_out = ok == 0 ? it : max_iter;
    free(r); free(w); free(beta_old);
    return ok;
}
