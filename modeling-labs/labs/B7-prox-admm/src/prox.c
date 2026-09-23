#include "prox.h"
#include "linalg.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

double mlab_soft_threshold(double z, double tau)
{
    if (z > tau) return z - tau;
    if (z < -tau) return z + tau;
    return 0.0;
}

static void XtXv(const double *X, int m, int n, const double *v, double *out)
{
    int i, j;
    for (j = 0; j < n; ++j) {
        double s = 0.0;
        for (i = 0; i < m; ++i) s += X[i * n + j] * v[i];
        out[j] = s;
    }
}

static void Xv(const double *X, int m, int n, const double *v, double *out)
{
    int i, j;
    for (i = 0; i < m; ++i) {
        double s = 0.0;
        for (j = 0; j < n; ++j) s += X[i * n + j] * v[j];
        out[i] = s;
    }
}

static double lasso_obj(const double *X, int m, int n, const double *y,
                        double lam, const double *w)
{
    double *Xw = (double *)malloc((size_t)m * sizeof(double));
    double s = 0.0;
    int i;
    if (!Xw) return 0;
    Xv(X, m, n, w, Xw);
    for (i = 0; i < m; ++i) {
        double d = Xw[i] - y[i];
        s += 0.5 * d * d;
    }
    for (i = 0; i < n; ++i) s += lam * fabs(w[i]);
    free(Xw);
    return s;
}

/* Lipschitz of grad = largest eig of X'X ~ power iteration */
static double lip_const(const double *X, int m, int n)
{
    double *v = (double *)malloc((size_t)n * sizeof(double));
    double *Xvtmp = (double *)malloc((size_t)m * sizeof(double));
    double *Xt = (double *)malloc((size_t)n * sizeof(double));
    double L = 1.0;
    int it, i;
    if (!v || !Xvtmp || !Xt) {
        free(v); free(Xvtmp); free(Xt);
        return 1.0;
    }
    for (i = 0; i < n; ++i) v[i] = 1.0 / sqrt((double)n);
    for (it = 0; it < 30; ++it) {
        double nrm = 0.0;
        Xv(X, m, n, v, Xvtmp);
        XtXv(X, m, n, Xvtmp, Xt); /* actually X'X v */
        nrm = mlab_nrm2(Xt, n);
        if (nrm < 1e-300) break;
        for (i = 0; i < n; ++i) v[i] = Xt[i] / nrm;
        L = nrm;
    }
    free(v); free(Xvtmp); free(Xt);
    return L > 1e-12 ? L : 1.0;
}

double mlab_ista(const double *X, int m, int n, const double *y,
                 double lam, double *w, int max_iter)
{
    double L = lip_const(X, m, n);
    double step = 1.0 / (L + 1e-12);
    double *grad = (double *)malloc((size_t)n * sizeof(double));
    double *Xw = (double *)malloc((size_t)m * sizeof(double));
    double *res = (double *)malloc((size_t)m * sizeof(double));
    int it, i, j;
    if (!grad || !Xw || !res) {
        free(grad); free(Xw); free(res);
        return 0;
    }
    for (it = 0; it < max_iter; ++it) {
        Xv(X, m, n, w, Xw);
        for (i = 0; i < m; ++i) res[i] = Xw[i] - y[i];
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (i = 0; i < m; ++i) s += X[i * n + j] * res[i];
            grad[j] = s;
        }
        for (j = 0; j < n; ++j)
            w[j] = mlab_soft_threshold(w[j] - step * grad[j], step * lam);
    }
    free(grad); free(Xw); free(res);
    return lasso_obj(X, m, n, y, lam, w);
}

double mlab_fista(const double *X, int m, int n, const double *y,
                  double lam, double *w, int max_iter)
{
    double L = lip_const(X, m, n);
    double step = 1.0 / (L + 1e-12);
    double *w_prev = (double *)calloc((size_t)n, sizeof(double));
    double *z = (double *)calloc((size_t)n, sizeof(double));
    double *grad = (double *)malloc((size_t)n * sizeof(double));
    double *Xz = (double *)malloc((size_t)m * sizeof(double));
    double *res = (double *)malloc((size_t)m * sizeof(double));
    double t = 1.0;
    int it, i, j;
    if (!w_prev || !z || !grad || !Xz || !res) {
        free(w_prev); free(z); free(grad); free(Xz); free(res);
        return 0;
    }
    memcpy(z, w, (size_t)n * sizeof(double));
    memcpy(w_prev, w, (size_t)n * sizeof(double));
    for (it = 0; it < max_iter; ++it) {
        double t_next;
        Xv(X, m, n, z, Xz);
        for (i = 0; i < m; ++i) res[i] = Xz[i] - y[i];
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (i = 0; i < m; ++i) s += X[i * n + j] * res[i];
            grad[j] = s;
        }
        for (j = 0; j < n; ++j)
            w[j] = mlab_soft_threshold(z[j] - step * grad[j], step * lam);
        t_next = 0.5 * (1.0 + sqrt(1.0 + 4.0 * t * t));
        for (j = 0; j < n; ++j) {
            z[j] = w[j] + ((t - 1.0) / t_next) * (w[j] - w_prev[j]);
            w_prev[j] = w[j];
        }
        t = t_next;
    }
    free(w_prev); free(z); free(grad); free(Xz); free(res);
    return lasso_obj(X, m, n, y, lam, w);
}

double mlab_admm_lasso(const double *X, int m, int n, const double *y,
                       double lam, double *w, int max_iter, double rho)
{
    /* x-update: (X'X + rho I) x = X'y + rho (z - u)
       z-update: soft(x+u, lam/rho)
       u-update: u + x - z */
    double *XtX = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *A = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *Xty = (double *)malloc((size_t)n * sizeof(double));
    double *rhs = (double *)malloc((size_t)n * sizeof(double));
    double *z = (double *)calloc((size_t)n, sizeof(double));
    double *u = (double *)calloc((size_t)n, sizeof(double));
    int it, i, j, k;
    if (!XtX || !A || !L || !Xty || !rhs || !z || !u) {
        free(XtX); free(A); free(L); free(Xty); free(rhs); free(z); free(u);
        return 0;
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
    if (rho <= 0) rho = 1.0;
    /* A = X'X + rho I 与 z,u 无关：Cholesky 只分解一次并复用 */
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            A[i * n + j] = XtX[i * n + j] + ((i == j) ? rho : 0.0);
    if (mlab_cholesky(A, n, L) != 0) {
        free(XtX); free(A); free(L); free(Xty); free(rhs); free(z); free(u);
        return -1.0; /* 目标非负：负返回值即分解失败 */
    }
    for (it = 0; it < max_iter; ++it) {
        for (i = 0; i < n; ++i)
            rhs[i] = Xty[i] + rho * (z[i] - u[i]);
        mlab_cholesky_solve(L, n, rhs, w);
        for (i = 0; i < n; ++i) {
            z[i] = mlab_soft_threshold(w[i] + u[i], lam / rho);
            u[i] = u[i] + w[i] - z[i];
        }
        /* report z as solution (sparse) */
    }
    memcpy(w, z, (size_t)n * sizeof(double));
    free(XtX); free(A); free(L); free(Xty); free(rhs); free(z); free(u);
    return lasso_obj(X, m, n, y, lam, w);
}

/* ---- 次梯度法（L327/L334） ---- */

double mlab_subgradient_lasso(const double *X, int m, int n, const double *y,
                              double lam, double *w, int max_iter, double alpha0)
{
    double *grad = (double *)malloc((size_t)n * sizeof(double));
    double *Xw = (double *)malloc((size_t)m * sizeof(double));
    double *res = (double *)malloc((size_t)m * sizeof(double));
    int it, i, j;
    if (!grad || !Xw || !res) {
        free(grad); free(Xw); free(res);
        return 0;
    }
    for (it = 0; it < max_iter; ++it) {
        double alpha = alpha0 / sqrt((double)(it + 1));
        Xv(X, m, n, w, Xw);
        for (i = 0; i < m; ++i) res[i] = Xw[i] - y[i];
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (i = 0; i < m; ++i) s += X[i * n + j] * res[i];
            grad[j] = s + lam * ((w[j] > 0) - (w[j] < 0)); /* 次微分 */
        }
        for (j = 0; j < n; ++j) w[j] -= alpha * grad[j];
    }
    free(grad); free(Xw); free(res);
    return lasso_obj(X, m, n, y, lam, w);
}

/* ---- 近端点法（L328） ---- */

double mlab_prox_point_lasso(const double *X, int m, int n, const double *y,
                             double lam, double *w, int outer, int inner,
                             double alpha)
{
    double L = lip_const(X, m, n);
    double step = 1.0 / (L + 1.0 / alpha); /* 近端项增大 Lipschitz 常数 */
    double *w_cur = (double *)malloc((size_t)n * sizeof(double));
    double *grad = (double *)malloc((size_t)n * sizeof(double));
    double *Xw = (double *)malloc((size_t)m * sizeof(double));
    double *res = (double *)malloc((size_t)m * sizeof(double));
    int o, t, i, j;
    if (!w_cur || !grad || !Xw || !res) {
        free(w_cur); free(grad); free(Xw); free(res);
        return 0;
    }
    memcpy(w_cur, w, (size_t)n * sizeof(double));
    for (o = 0; o < outer; ++o) {
        memcpy(w, w_cur, (size_t)n * sizeof(double));
        for (t = 0; t < inner; ++t) {
            /* 近端子问题：min 0.5||Xw-y||^2 + lam||w||_1 + (1/2a)||w - w_cur||^2 */
            Xv(X, m, n, w, Xw);
            for (i = 0; i < m; ++i) res[i] = Xw[i] - y[i];
            for (j = 0; j < n; ++j) {
                double s = 0.0;
                for (i = 0; i < m; ++i) s += X[i * n + j] * res[i];
                grad[j] = s + (w[j] - w_cur[j]) / alpha;
            }
            for (j = 0; j < n; ++j)
                w[j] = mlab_soft_threshold(w[j] - step * grad[j], step * lam);
        }
        memcpy(w_cur, w, (size_t)n * sizeof(double));
    }
    free(w_cur); free(grad); free(Xw); free(res);
    return lasso_obj(X, m, n, y, lam, w);
}

static void matvec(const double *A, int m, int n, const double *v, double *out)
{
    int i, j;
    for (i = 0; i < m; ++i) {
        double s = 0.0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * v[j];
        out[i] = s;
    }
}

static void matTvec(const double *A, int m, int n, const double *v, double *out)
{
    int i, j;
    for (j = 0; j < n; ++j) {
        double s = 0.0;
        for (i = 0; i < m; ++i) s += A[i * n + j] * v[i];
        out[j] = s;
    }
}

/* ---- 基追踪 BP（L331）：min ||z||_1 s.t. Xw = y ---- */

double mlab_bp_admm(const double *X, int m, int n, const double *y,
                    double *w, int max_iter, double rho)
{
    /* Boyd 标准两块 ADMM：
       x-update：min (rho/2)||x - z + u||^2 s.t. Xx = y
                 -> x = (z - u) - X'(XX')^{-1}(X(z-u) - y)  （仿射集投影）
       z-update：z = soft(x + u, 1/rho)
       u-update：u <- u + x - z */
    double *XXt = (double *)malloc((size_t)m * m * sizeof(double));
    double *L = (double *)malloc((size_t)m * m * sizeof(double));
    double *u = (double *)calloc((size_t)n, sizeof(double));
    double *z = (double *)calloc((size_t)n, sizeof(double));
    double *x = (double *)malloc((size_t)n * sizeof(double));
    double *tmp = (double *)malloc((size_t)m * sizeof(double));
    double *coef = (double *)malloc((size_t)m * sizeof(double));
    double *xv = (double *)malloc((size_t)n * sizeof(double)); /* X'coef 为 n 维 */
    int it, i, j, k;
    double obj = 0.0;
    if (!XXt || !L || !u || !z || !x || !tmp || !coef || !xv) {
        free(XXt); free(L); free(u); free(z); free(x); free(tmp); free(coef); free(xv);
        return -1.0;
    }
    if (rho <= 0) rho = 1.0;
    for (i = 0; i < m; ++i)
        for (j = 0; j < m; ++j) {
            double acc = 0.0;
            for (k = 0; k < n; ++k) acc += X[i * n + k] * X[j * n + k];
            XXt[i * m + j] = acc;
        }
    if (mlab_cholesky(XXt, m, L) != 0) {
        free(XXt); free(L); free(u); free(z); free(x); free(tmp); free(coef);
        return -1.0;
    }
    for (it = 0; it < max_iter; ++it) {
        /* x = (z-u) - X'(XX')^{-1}(X(z-u) - y) */
        for (k = 0; k < n; ++k) x[k] = z[k] - u[k];
        matvec(X, m, n, x, tmp);
        for (i = 0; i < m; ++i) tmp[i] -= y[i];
        mlab_cholesky_solve(L, m, tmp, coef);
        matTvec(X, m, n, coef, xv); /* X'coef 为 n 维向量（此前误写入 m 维缓冲） */
        for (k = 0; k < n; ++k) x[k] -= xv[k];
        /* z = soft(x + u, 1/rho)；u 更新 */
        for (k = 0; k < n; ++k) z[k] = mlab_soft_threshold(x[k] + u[k], 1.0 / rho);
        for (k = 0; k < n; ++k) u[k] += x[k] - z[k];
    }
    memcpy(w, x, (size_t)n * sizeof(double));
    obj = 0.0;
    for (k = 0; k < n; ++k) obj += fabs(w[k]);
    free(XXt); free(L); free(u); free(z); free(x); free(tmp); free(coef); free(xv);
    return obj;
}

/* ---- ADMM 带等式约束 QP（L336/341）：min 0.5||x||^2 s.t. A x = b ---- */

int mlab_admm_qp_eq(const double *A, int m, int n, const double *b,
                    double rho, int max_iter, double tol, double *x_out)
{
    /* x-update: x = (z - u)/(1 + rho)（P=I）；
       z-update: 仿射集投影 proj_{Az=b}(x+u) = z0 + A'(AA')^{-1}(A(x+u)-b)，
       z0 = A'(AA')^{-1} b 为最小范数解。 */
    double *AAt = (double *)malloc((size_t)m * m * sizeof(double));
    double *L = (double *)malloc((size_t)m * m * sizeof(double));
    double *z = (double *)calloc((size_t)n, sizeof(double));
    double *u = (double *)calloc((size_t)n, sizeof(double));
    double *z0 = (double *)malloc((size_t)n * sizeof(double));
    double *tmp = (double *)malloc((size_t)m * sizeof(double));
    double *c2 = (double *)malloc((size_t)m * sizeof(double));
    double *x = (double *)malloc((size_t)n * sizeof(double));
    double *xu = (double *)malloc((size_t)n * sizeof(double));
    int it, i, k, status = 1;
    if (!AAt || !L || !z || !u || !z0 || !tmp || !c2 || !x || !xu) {
        free(AAt); free(L); free(z); free(u); free(z0); free(tmp); free(c2); free(x); free(xu);
        return -1;
    }
    if (rho <= 0) rho = 1.0;
    for (i = 0; i < m; ++i)
        for (k = 0; k < m; ++k) {
            double acc = 0.0;
            int q;
            for (q = 0; q < n; ++q) acc += A[i * n + q] * A[k * n + q];
            AAt[i * m + k] = acc;
        }
    if (mlab_cholesky(AAt, m, L) != 0) {
        free(AAt); free(L); free(z); free(u); free(z0); free(tmp); free(c2); free(x); free(xu);
        return -1;
    }
    mlab_cholesky_solve(L, m, b, tmp); /* tmp <- (AA')^{-1} b */
    for (k = 0; k < n; ++k) {
        double s = 0.0;
        for (i = 0; i < m; ++i) s += A[i * n + k] * tmp[i];
        z0[k] = s;
    }
    for (it = 0; it < max_iter; ++it) {
        double rnorm = 0.0;
        for (k = 0; k < n; ++k) x[k] = (z[k] - u[k]) / (1.0 + rho);
        for (k = 0; k < n; ++k) xu[k] = x[k] + u[k];
        matvec(A, m, n, xu, tmp);
        for (i = 0; i < m; ++i) tmp[i] -= b[i];
        mlab_cholesky_solve(L, m, tmp, c2);
        matTvec(A, m, n, c2, z);
        for (k = 0; k < n; ++k) z[k] = xu[k] - z[k]; /* 投影 */
        for (k = 0; k < n; ++k) {
            double dk = x[k] - z[k];
            rnorm += dk * dk;
        }
        for (k = 0; k < n; ++k) u[k] += x[k] - z[k];
        if (sqrt(rnorm) < tol) {
            status = 0;
            break;
        }
    }
    memcpy(x_out, z, (size_t)n * sizeof(double));
    free(AAt); free(L); free(z); free(u); free(z0); free(tmp); free(c2); free(x); free(xu);
    return status;
}
