#include "ode.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 固定步长 ---------- */

int mlab_ode_euler(mlab_ode_rhs f, void *ctx, int n,
                   double t0, double t1, double h, double *y)
{
    double t = t0;
    double *dydt;
    int i, steps;
    if (!f || n <= 0 || !y || h <= 0.0 || t1 < t0) return -1;
    dydt = (double *)malloc((size_t)n * sizeof(double));
    if (!dydt) return -1;
    steps = (int)ceil((t1 - t0) / h - 1e-12);
    if (steps < 0) steps = 0;
    for (i = 0; i < steps; ++i) {
        double hstep = h;
        int k;
        if (t + hstep > t1) hstep = t1 - t;
        if (hstep <= 0.0) break;
        f(t, y, dydt, ctx);
        for (k = 0; k < n; ++k) y[k] += hstep * dydt[k];
        t += hstep;
    }
    free(dydt);
    return 0;
}

int mlab_ode_rk4(mlab_ode_rhs f, void *ctx, int n,
                 double t0, double t1, double h, double *y)
{
    double t = t0;
    double *k1, *k2, *k3, *k4, *tmp;
    int i, steps;
    if (!f || n <= 0 || !y || h <= 0.0 || t1 < t0) return -1;
    k1 = (double *)malloc((size_t)n * 4 * sizeof(double));
    tmp = (double *)malloc((size_t)n * sizeof(double));
    if (!k1 || !tmp) {
        free(k1);
        free(tmp);
        return -1;
    }
    k2 = k1 + n;
    k3 = k2 + n;
    k4 = k3 + n;
    steps = (int)ceil((t1 - t0) / h - 1e-12);
    if (steps < 0) steps = 0;
    for (i = 0; i < steps; ++i) {
        double hstep = h;
        int k;
        if (t + hstep > t1) hstep = t1 - t;
        if (hstep <= 0.0) break;
        f(t, y, k1, ctx);
        for (k = 0; k < n; ++k) tmp[k] = y[k] + 0.5 * hstep * k1[k];
        f(t + 0.5 * hstep, tmp, k2, ctx);
        for (k = 0; k < n; ++k) tmp[k] = y[k] + 0.5 * hstep * k2[k];
        f(t + 0.5 * hstep, tmp, k3, ctx);
        for (k = 0; k < n; ++k) tmp[k] = y[k] + hstep * k3[k];
        f(t + hstep, tmp, k4, ctx);
        for (k = 0; k < n; ++k)
            y[k] += (hstep / 6.0) * (k1[k] + 2.0 * k2[k] + 2.0 * k3[k] + k4[k]);
        t += hstep;
    }
    free(k1);
    free(tmp);
    return 0;
}

/* 数值雅可比：J = ∂f/∂y */
static void fd_jacobian(mlab_ode_rhs f, void *ctx, double t, const double *y,
                        int n, double *J)
{
    double *yj = (double *)malloc((size_t)n * sizeof(double));
    double *fp = (double *)malloc((size_t)n * sizeof(double));
    double *fm = (double *)malloc((size_t)n * sizeof(double));
    int i, j;
    if (!yj || !fp || !fm) {
        free(yj);
        free(fp);
        free(fm);
        return;
    }
    for (j = 0; j < n; ++j) {
        double eps, yj_save;
        memcpy(yj, y, (size_t)n * sizeof(double));
        eps = 1e-6 * (fabs(y[j]) + 1.0);
        yj_save = yj[j];
        yj[j] = yj_save + eps;
        f(t, yj, fp, ctx);
        yj[j] = yj_save - eps;
        f(t, yj, fm, ctx);
        yj[j] = yj_save;
        for (i = 0; i < n; ++i)
            J[i * n + j] = (fp[i] - fm[i]) / (2.0 * eps);
    }
    free(yj);
    free(fp);
    free(fm);
}

/* 解 (I - h J) delta = residual，高斯消元 */
static int solve_dense(double *A, int n, double *b, double *x)
{
    int i, j, k;
    for (k = 0; k < n; ++k) {
        int piv = k;
        double amax = fabs(A[k * n + k]);
        for (i = k + 1; i < n; ++i) {
            double v = fabs(A[i * n + k]);
            if (v > amax) {
                amax = v;
                piv = i;
            }
        }
        if (amax < 1e-300) return -1;
        if (piv != k) {
            for (j = 0; j < n; ++j) {
                double tmp = A[k * n + j];
                A[k * n + j] = A[piv * n + j];
                A[piv * n + j] = tmp;
            }
            {
                double tmp = b[k];
                b[k] = b[piv];
                b[piv] = tmp;
            }
        }
        for (i = k + 1; i < n; ++i) {
            double m = A[i * n + k] / A[k * n + k];
            for (j = k; j < n; ++j) A[i * n + j] -= m * A[k * n + j];
            b[i] -= m * b[k];
        }
    }
    for (i = n - 1; i >= 0; --i) {
        double s = b[i];
        for (j = i + 1; j < n; ++j) s -= A[i * n + j] * x[j];
        x[i] = s / A[i * n + i];
    }
    return 0;
}

int mlab_ode_euler_implicit(mlab_ode_rhs f, void *ctx, int n,
                            double t0, double t1, double h, double *y,
                            int max_newton, double newton_tol)
{
    double t = t0;
    double *yold = NULL, *yn = NULL, *fnew = NULL, *res = NULL, *J = NULL, *A = NULL, *dx = NULL;
    int steps, i, it;
    if (!f || n <= 0 || !y || h <= 0.0 || t1 < t0) return -1;
    if (max_newton <= 0) max_newton = 20;
    if (newton_tol <= 0.0) newton_tol = 1e-10;

    yold = (double *)malloc((size_t)n * sizeof(double));
    yn = (double *)malloc((size_t)n * sizeof(double));
    fnew = (double *)malloc((size_t)n * sizeof(double));
    res = (double *)malloc((size_t)n * sizeof(double));
    J = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    A = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    dx = (double *)malloc((size_t)n * sizeof(double));
    if (!yold || !yn || !fnew || !res || !J || !A || !dx) {
        free(yold); free(yn); free(fnew); free(res);
        free(J); free(A); free(dx);
        return -1;
    }

    steps = (int)ceil((t1 - t0) / h - 1e-12);
    if (steps < 0) steps = 0;
    for (i = 0; i < steps; ++i) {
        double hstep = h;
        int k;
        if (t + hstep > t1) hstep = t1 - t;
        if (hstep <= 0.0) break;
        memcpy(yold, y, (size_t)n * sizeof(double));
        memcpy(yn, y, (size_t)n * sizeof(double));
        for (it = 0; it < max_newton; ++it) {
            double nrm = 0.0;
            f(t + hstep, yn, fnew, ctx);
            fd_jacobian(f, ctx, t + hstep, yn, n, J);
            /* residual R = y - yold - h*f(t+h,y); solve (I-hJ) dx = -R
               Newton: y := y - (I-hJ)^{-1} R */
            for (k = 0; k < n; ++k) {
                res[k] = yn[k] - yold[k] - hstep * fnew[k];
                nrm += res[k] * res[k];
            }
            nrm = sqrt(nrm);
            if (nrm < newton_tol) break;
            for (k = 0; k < n * n; ++k) A[k] = -hstep * J[k];
            for (k = 0; k < n; ++k) A[k * n + k] += 1.0;
            for (k = 0; k < n; ++k) res[k] = -res[k];
            if (solve_dense(A, n, res, dx) != 0) {
                free(yold); free(yn); free(fnew); free(res);
                free(J); free(A); free(dx);
                return -2;
            }
            for (k = 0; k < n; ++k) yn[k] += dx[k];
        }
        if (it == max_newton) {
            /* Newton 未收敛：显式返回码，不静默接受未解状态 */
            double nrm = 0.0;
            f(t + hstep, yn, fnew, ctx);
            for (k = 0; k < n; ++k) {
                double r = yn[k] - yold[k] - hstep * fnew[k];
                nrm += r * r;
            }
            if (sqrt(nrm) >= newton_tol) {
                free(yold); free(yn); free(fnew); free(res);
                free(J); free(A); free(dx);
                return -3;
            }
        }
        memcpy(y, yn, (size_t)n * sizeof(double));
        t += hstep;
    }
    free(yold); free(yn); free(fnew); free(res);
    free(J); free(A); free(dx);
    return 0;
}

/* ---------- RKF45 ---------- */
/* Fehlberg 1969 coefficients */
static const double rkf_c[6] = {
    0.0, 0.25, 3.0/8.0, 12.0/13.0, 1.0, 0.5
};
static const double rkf_a[6][5] = {
    {0,0,0,0,0},
    {0.25,0,0,0,0},
    {3.0/32.0, 9.0/32.0, 0,0,0},
    {1932.0/2197.0, -7200.0/2197.0, 7296.0/2197.0, 0, 0},
    {439.0/216.0, -8.0, 3680.0/513.0, -845.0/4104.0, 0},
    {-8.0/27.0, 2.0, -3544.0/2565.0, 1859.0/4104.0, -11.0/40.0}
};
/* 4th order weights */
static const double rkf_b4[6] = {
    25.0/216.0, 0.0, 1408.0/2565.0, 2197.0/4104.0, -0.2, 0.0
};
/* 5th order weights */
static const double rkf_b5[6] = {
    16.0/135.0, 0.0, 6656.0/12825.0, 28561.0/56430.0, -9.0/50.0, 2.0/55.0
};

/* 六级stage评估 + 4/5 阶解与未缩放误差分量（step/trace 共用） */
static void rkf45_stages(mlab_ode_rhs f, void *ctx, int n,
                         double t, const double *y, double h,
                         double **k, double *y4, double *y5,
                         double *err_raw, int *fev)
{
    int s, i, a;
    for (s = 0; s < 6; ++s) {
        int j;
        for (j = 0; j < n; ++j) y5[j] = y[j]; /* y5 暂作 stage 输入缓冲 */
        for (a = 0; a < s; ++a) {
            double coef = rkf_a[s][a];
            if (coef == 0.0) continue;
            for (j = 0; j < n; ++j) y5[j] += h * coef * k[a][j];
        }
        f(t + rkf_c[s] * h, y5, k[s], ctx);
        ++(*fev);
    }
    for (i = 0; i < n; ++i) {
        double e4 = 0.0, e5 = 0.0;
        for (a = 0; a < 6; ++a) {
            e4 += rkf_b4[a] * k[a][i];
            e5 += rkf_b5[a] * k[a][i];
        }
        y4[i] = y[i] + h * e4;
        y5[i] = y[i] + h * e5;
    }
    {
        double m = 0.0;
        for (i = 0; i < n; ++i) {
            double e = fabs(y5[i] - y4[i]);
            if (e > m) m = e;
        }
        *err_raw = m;
    }
}

int mlab_ode_rkf45_step(mlab_ode_rhs f, void *ctx, int n,
                        double t, double *y, double h,
                        double rtol, double atol, double *err_est)
{
    double *k[6], *y4, *y5;
    double err_norm = 0.0, err_raw;
    int s, i, fev = 0;
    if (!f || n <= 0 || !y || !(h > 0.0)) return -1;
    if (rtol <= 0.0) rtol = 1e-6;
    if (atol <= 0.0) atol = 1e-8;
    y4 = (double *)malloc((size_t)n * sizeof(double));
    y5 = (double *)malloc((size_t)n * sizeof(double));
    for (s = 0; s < 6; ++s) k[s] = NULL;
    for (s = 0; s < 6; ++s) {
        k[s] = (double *)malloc((size_t)n * sizeof(double));
        if (!k[s] || !y4 || !y5) {
            for (i = 0; i < 6; ++i) free(k[i]);
            free(y4); free(y5);
            return -1;
        }
    }
    rkf45_stages(f, ctx, n, t, y, h, k, y4, y5, &err_raw, &fev);
    for (i = 0; i < n; ++i) {
        double sc = atol + rtol * fmax(fabs(y[i]), fabs(y5[i]));
        double e = fabs(y5[i] - y4[i]) / sc;
        if (e > err_norm) err_norm = e;
    }
    if (err_est) *err_est = err_raw;
    if (err_norm <= 1.0) {
        memcpy(y, y5, (size_t)n * sizeof(double));
        for (i = 0; i < 6; ++i) free(k[i]);
        free(y4); free(y5);
        return 1;
    }
    for (i = 0; i < 6; ++i) free(k[i]);
    free(y4); free(y5);
    return 0;
}

void mlab_rkf45_trace_free(mlab_rkf45_trace *tr)
{
    if (!tr) return;
    free(tr->t);
    free(tr->h);
    free(tr->err);
    tr->t = tr->h = tr->err = NULL;
    tr->n = tr->cap = 0;
}

static int trace_push(mlab_rkf45_trace *tr, double t, double h, double err)
{
    if (!tr) return 0;
    if (tr->n >= tr->cap) {
        int ncap = tr->cap > 0 ? tr->cap * 2 : 128;
        double *nt = (double *)realloc(tr->t, (size_t)ncap * sizeof(double));
        double *nh = (double *)realloc(tr->h, (size_t)ncap * sizeof(double));
        double *ne = (double *)realloc(tr->err, (size_t)ncap * sizeof(double));
        if (!nt || !nh || !ne) {
            free(nt); free(nh); free(ne);
            return -1;
        }
        tr->t = nt; tr->h = nh; tr->err = ne;
        tr->cap = ncap;
    }
    tr->t[tr->n] = t;
    tr->h[tr->n] = h;
    tr->err[tr->n] = err;
    ++tr->n;
    return 0;
}

int mlab_ode_rkf45_trace(mlab_ode_rhs f, void *ctx, int n,
                         double t0, double t1, double *y,
                         double rtol, double atol,
                         double h_init, mlab_rkf45_stats *stats,
                         mlab_rkf45_trace *trace)
{
    double t = t0, h;
    double *k[6], *ytmp, *y4, *y5;
    int i, s, iter, fev;
    const int max_iter = 2000000;
    if (!f || n <= 0 || !y || t1 < t0) return -1;
    if (rtol <= 0.0) rtol = 1e-6;
    if (atol <= 0.0) atol = 1e-8;
    if (h_init <= 0.0) h_init = fmin(0.01, (t1 - t0) > 0 ? (t1 - t0) * 0.01 : 0.01);
    if (stats) memset(stats, 0, sizeof *stats);
    if (trace) { trace->t = trace->h = trace->err = NULL; trace->n = trace->cap = 0; }

    ytmp = (double *)malloc((size_t)n * sizeof(double));
    y4 = (double *)malloc((size_t)n * sizeof(double));
    y5 = (double *)malloc((size_t)n * sizeof(double));
    for (s = 0; s < 6; ++s) {
        k[s] = (double *)malloc((size_t)n * sizeof(double));
        if (!k[s] || !ytmp || !y4 || !y5) {
            free(ytmp); free(y4); free(y5);
            for (i = 0; i < s; ++i) free(k[i]);
            return -1;
        }
    }

    h = h_init;
    if (stats) {
        stats->h_min = 1e300;
        stats->h_max = 0.0;
    }

    for (iter = 0; iter < max_iter && t < t1 - 1e-15; ++iter) {
        double err_norm = 0.0, err_raw, h_try;
        int accept = 0;
        if (t + h > t1) h = t1 - t;
        if (h <= 0.0) break;
        h_try = h;

        rkf45_stages(f, ctx, n, t, y, h_try, k, y4, y5, &err_raw, &fev);
        if (stats) stats->n_fev += fev;
        for (i = 0; i < n; ++i) {
            double sc = atol + rtol * fmax(fabs(y[i]), fabs(y5[i]));
            double e = fabs(y5[i] - y4[i]) / sc;
            if (e > err_norm) err_norm = e;
        }

        if (err_norm <= 1.0) {
            memcpy(y, y5, (size_t)n * sizeof(double));
            t += h_try;
            accept = 1;
            if (stats) {
                ++stats->n_accept;
                if (err_raw > stats->max_err_est) stats->max_err_est = err_raw;
                if (h_try < stats->h_min) stats->h_min = h_try;
                if (h_try > stats->h_max) stats->h_max = h_try;
            }
            if (trace && trace_push(trace, t - h_try, h_try, err_raw) != 0) {
                free(ytmp); free(y4); free(y5);
                for (i = 0; i < 6; ++i) free(k[i]);
                return -1;
            }
        } else if (stats) {
            ++stats->n_reject;
        }

        /* 步长更新：safety * err^(-1/5) */
        {
            double factor;
            if (err_norm <= 0.0) factor = 2.0;
            else factor = 0.9 * pow(1.0 / err_norm, 0.2);
            if (factor > 2.0) factor = 2.0;
            if (factor < 0.2) factor = 0.2;
            h = h_try * factor;
            if (!accept && h < 1e-14 * (fabs(t) + 1.0)) {
                free(ytmp); free(y4); free(y5);
                for (i = 0; i < 6; ++i) free(k[i]);
                return -2;
            }
        }
    }

    free(ytmp);
    free(y4);
    free(y5);
    for (i = 0; i < 6; ++i) free(k[i]);
    return 0;
}

int mlab_ode_rkf45(mlab_ode_rhs f, void *ctx, int n,
                   double t0, double t1, double *y,
                   double rtol, double atol,
                   double h_init, mlab_rkf45_stats *stats)
{
    return mlab_ode_rkf45_trace(f, ctx, n, t0, t1, y, rtol, atol,
                                h_init, stats, NULL);
}

/* ---------- 模板 ---------- */

void mlab_harm_rhs(double t, const double *y, double *dydt, void *ctx)
{
    const mlab_harm_ctx *c = (const mlab_harm_ctx *)ctx;
    double w2 = (c && c->omega != 0.0) ? c->omega * c->omega : 1.0;
    (void)t;
    dydt[0] = y[1];
    dydt[1] = -w2 * y[0];
}

double mlab_harm_energy(const double *y, const mlab_harm_ctx *ctx)
{
    double w2 = (ctx && ctx->omega != 0.0) ? ctx->omega * ctx->omega : 1.0;
    return 0.5 * (y[1] * y[1] + w2 * y[0] * y[0]);
}

void mlab_lv_rhs(double t, const double *y, double *dydt, void *ctx)
{
    const mlab_lv_ctx *c = (const mlab_lv_ctx *)ctx;
    double a = c->alpha, b = c->beta, g = c->gamma, d = c->delta;
    (void)t;
    dydt[0] = a * y[0] - b * y[0] * y[1];
    dydt[1] = d * y[0] * y[1] - g * y[1];
}

void mlab_seir_rhs(double t, const double *y, double *dydt, void *ctx)
{
    const mlab_seir_ctx *c = (const mlab_seir_ctx *)ctx;
    double S = y[0], E = y[1], I = y[2];
    double N = c->N > 0 ? c->N : 1.0;
    (void)t;
    dydt[0] = -c->beta * S * I / N;
    dydt[1] = c->beta * S * I / N - c->sigma * E;
    dydt[2] = c->sigma * E - c->gamma * I;
    dydt[3] = c->gamma * I;
}

void mlab_robertson_rhs(double t, const double *y, double *dydt, void *ctx)
{
    (void)t;
    (void)ctx;
    dydt[0] = -0.04 * y[0] + 1.0e4 * y[1] * y[2];
    dydt[1] = 0.04 * y[0] - 1.0e4 * y[1] * y[2] - 3.0e7 * y[1] * y[1];
    dydt[2] = 3.0e7 * y[1] * y[1];
}

/* ---------- 药代动力学房室模板 ---------- */

void mlab_pk1_rhs(double t, const double *y, double *dydt, void *ctx)
{
    const mlab_pk1_ctx *c = (const mlab_pk1_ctx *)ctx;
    (void)t;
    dydt[0] = -c->kel * y[0];
}

double mlab_pk1_analytic(double t, double C0, double kel)
{
    return C0 * exp(-kel * t);
}

void mlab_pk2_rhs(double t, const double *y, double *dydt, void *ctx)
{
    const mlab_pk2_ctx *c = (const mlab_pk2_ctx *)ctx;
    (void)t;
    dydt[0] = -c->ka * y[0];
    dydt[1] = c->ka * y[0] - (c->kel + c->k12) * y[1] + c->k21 * y[2];
    dydt[2] = c->k12 * y[1] - c->k21 * y[2];
}

int mlab_pk2_analytic(const mlab_pk2_ctx *c, const double *y0, double t,
                      double *y_out)
{
    double S, D, sq, lam1, lam2;
    double denom, C_a, D_a, rhs1, rhs2, det, C1, C2, D1, D2;
    double ea, e1, e2;
    if (!c || !y0 || !y_out) return -1;
    S = c->kel + c->k12 + c->k21;
    D = S * S - 4.0 * c->kel * c->k21;
    if (D < 0.0) return -1; /* 实参数下不会发生，防御 */
    sq = sqrt(D);
    lam1 = 0.5 * (-S + sq);
    lam2 = 0.5 * (-S - sq);
    /* 退化守卫：ka/k21 与特征值重合时部分分式失效 */
    if (fabs(c->ka + lam1) < 1e-9 || fabs(c->ka + lam2) < 1e-9 ||
        fabs(c->k21 + lam1) < 1e-9 || fabs(c->k21 + lam2) < 1e-9 ||
        fabs(c->k21 - c->ka) < 1e-9)
        return -1;
    /* 强迫项（e^{-ka t}）系数：由 (1)(2) 匹配 e^{-ka t} 分量 */
    denom = c->ka * c->ka - c->ka * S + c->kel * c->k21; /* = (ka-μ1)(ka-μ2) */
    C_a = c->ka * y0[0] * (c->k21 - c->ka) / denom;
    D_a = c->k12 * C_a / (c->k21 - c->ka);
    /* 齐次系数 C1,C2：A_1(0) 与 A_1'(0)（后者由 ODE 右端给出，等价于 A_2(0)） */
    rhs1 = y0[1] - C_a;
    rhs2 = c->ka * y0[0] - (c->kel + c->k12) * y0[1] + c->k21 * y0[2]
           + c->ka * C_a;
    det = lam1 - lam2;
    C1 = (rhs2 - lam2 * rhs1) / det;
    C2 = (lam1 * rhs1 - rhs2) / det;
    D1 = c->k12 * C1 / (lam1 + c->k21);
    D2 = c->k12 * C2 / (lam2 + c->k21);
    ea = exp(-c->ka * t);
    e1 = exp(lam1 * t);
    e2 = exp(lam2 * t);
    y_out[0] = y0[0] * ea;
    y_out[1] = C_a * ea + C1 * e1 + C2 * e2;
    y_out[2] = D_a * ea + D1 * e1 + D2 * e2;
    return 0;
}

/* ---------- 敏感性 ---------- */

static void theta_bridge(double t, const double *y, double *dydt, void *ctx)
{
    /* ctx 打包：struct { mlab_ode_rhs_theta f; void *user; const double *theta; int ntheta; } */
    typedef struct {
        mlab_ode_rhs_theta f;
        void *user;
        const double *theta;
        int ntheta;
    } pack;
    pack *p = (pack *)ctx;
    p->f(t, y, dydt, p->theta, p->ntheta, p->user);
}

int mlab_ode_sensitivity_fd(mlab_ode_rhs_theta f, void *ctx, int n,
                            const double *theta, int ntheta,
                            double t0, double t1, double h,
                            const double *y0, double eps_rel,
                            double *S_out, double *y_nominal)
{
    typedef struct {
        mlab_ode_rhs_theta f;
        void *user;
        const double *theta;
        int ntheta;
    } pack;
    pack p;
    double *th = NULL, *yplus = NULL, *yminus = NULL, *yn = NULL;
    int j, i, rc = 0;
    if (!f || n <= 0 || ntheta <= 0 || !theta || !y0 || !S_out) return -1;
    if (eps_rel <= 0.0) eps_rel = 1e-6;

    th = (double *)malloc((size_t)ntheta * sizeof(double));
    yplus = (double *)malloc((size_t)n * sizeof(double));
    yminus = (double *)malloc((size_t)n * sizeof(double));
    yn = (double *)malloc((size_t)n * sizeof(double));
    if (!th || !yplus || !yminus || !yn) {
        free(th); free(yplus); free(yminus); free(yn);
        return -1;
    }
    memcpy(th, theta, (size_t)ntheta * sizeof(double));
    p.f = f;
    p.user = ctx;
    p.theta = th;
    p.ntheta = ntheta;

    memcpy(yn, y0, (size_t)n * sizeof(double));
    if (mlab_ode_rk4(theta_bridge, &p, n, t0, t1, h, yn) != 0) {
        free(th); free(yplus); free(yminus); free(yn);
        return -1;
    }
    if (y_nominal) memcpy(y_nominal, yn, (size_t)n * sizeof(double));

    for (j = 0; j < ntheta; ++j) {
        double eps = eps_rel * (fabs(theta[j]) + 1.0);
        memcpy(th, theta, (size_t)ntheta * sizeof(double));
        th[j] = theta[j] + eps;
        memcpy(yplus, y0, (size_t)n * sizeof(double));
        if (mlab_ode_rk4(theta_bridge, &p, n, t0, t1, h, yplus) != 0) {
            rc = -1;
            break;
        }
        th[j] = theta[j] - eps;
        memcpy(yminus, y0, (size_t)n * sizeof(double));
        if (mlab_ode_rk4(theta_bridge, &p, n, t0, t1, h, yminus) != 0) {
            rc = -1;
            break;
        }
        for (i = 0; i < n; ++i)
            S_out[i + j * n] = (yplus[i] - yminus[i]) / (2.0 * eps);
    }
    free(th); free(yplus); free(yminus); free(yn);
    return rc;
}
