#include "nlp.h"
#include "linalg.h"
#include "optcore.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const mlab_objective *base;
    mlab_ineq_fn ineq;
    int m_ineq;
    void *ineq_ctx;
    mlab_eq_fn eq;
    int m_eq;
    void *eq_ctx;
    double mu;
    double sigma; /* 0 => pure quadratic penalty */
    const double *lam; /* AL multipliers for ineq */
    const double *nu;
} pen_ctx;

/* 约束梯度（中心差分）：取第 i 个不等式/等式约束的梯度列 */
static void ineq_grad_fd(const pen_ctx *p, int idx, const double *x, int n, double *gcol)
{
    int j;
    double *xp = (double *)malloc((size_t)n * sizeof(double));
    double *gp = (double *)malloc((size_t)p->m_ineq * sizeof(double));
    double *gm = (double *)malloc((size_t)p->m_ineq * sizeof(double));
    if (!xp || !gp || !gm) {
        free(xp); free(gp); free(gm);
        return;
    }
    for (j = 0; j < n; ++j) {
        double h = 1e-6 * (fabs(x[j]) > 1.0 ? fabs(x[j]) : 1.0);
        memcpy(xp, x, (size_t)n * sizeof(double));
        xp[j] += h;
        p->ineq(xp, n, gp, p->m_ineq, p->ineq_ctx);
        xp[j] = x[j] - h;
        p->ineq(xp, n, gm, p->m_ineq, p->ineq_ctx);
        gcol[j] = (gp[idx] - gm[idx]) / (2.0 * h);
    }
    free(xp); free(gp); free(gm);
}

static void eq_grad_fd(const pen_ctx *p, int idx, const double *x, int n, double *gcol)
{
    int j;
    double *xp = (double *)malloc((size_t)n * sizeof(double));
    double *hp = (double *)malloc((size_t)p->m_eq * sizeof(double));
    double *hm = (double *)malloc((size_t)p->m_eq * sizeof(double));
    if (!xp || !hp || !hm) {
        free(xp); free(hp); free(hm);
        return;
    }
    for (j = 0; j < n; ++j) {
        double h = 1e-6 * (fabs(x[j]) > 1.0 ? fabs(x[j]) : 1.0);
        memcpy(xp, x, (size_t)n * sizeof(double));
        xp[j] += h;
        p->eq(xp, n, hp, p->m_eq, p->eq_ctx);
        xp[j] = x[j] - h;
        p->eq(xp, n, hm, p->m_eq, p->eq_ctx);
        gcol[j] = (hp[idx] - hm[idx]) / (2.0 * h);
    }
    free(xp); free(hp); free(hm);
}

static double pen_f(const double *x, int n, void *ctx)
{
    pen_ctx *p = (pen_ctx *)ctx;
    double s = p->base->f(x, n, p->base->ctx);
    double *g = NULL, *h = NULL;
    int i;
    if (p->m_ineq > 0 && p->ineq) {
        g = (double *)malloc((size_t)p->m_ineq * sizeof(double));
        if (g) {
            p->ineq(x, n, g, p->m_ineq, p->ineq_ctx);
            for (i = 0; i < p->m_ineq; ++i) {
                double v = g[i];
                if (v < 0) v = 0;
                if (p->sigma > 0 && p->lam) {
                    /* AL ineq term */
                    double li = p->lam[i];
                    double t = li + p->sigma * g[i];
                    if (t < 0) t = 0;
                    s += (t * t - li * li) / (2.0 * p->sigma);
                } else {
                    s += p->mu * v * v;
                }
            }
            free(g);
        }
    }
    if (p->m_eq > 0 && p->eq) {
        h = (double *)malloc((size_t)p->m_eq * sizeof(double));
        if (h) {
            double mu = p->sigma > 0 ? p->sigma : p->mu;
            p->eq(x, n, h, p->m_eq, p->eq_ctx);
            for (i = 0; i < p->m_eq; ++i) {
                if (p->sigma > 0 && p->nu) {
                    s += p->nu[i] * h[i] + 0.5 * p->sigma * h[i] * h[i];
                } else {
                    s += mu * h[i] * h[i];
                }
            }
            free(h);
        }
    }
    return s;
}

/* 罚目标梯度：基础目标用解析梯度，约束梯度用中心差分（精度 ~1e-9，
   远优于此前对整个罚目标做前向差分的 ~1e-6） */
static void pen_g(const double *x, int n, double *grad, void *ctx)
{
    pen_ctx *p = (pen_ctx *)ctx;
    double *gcol = (double *)malloc((size_t)n * sizeof(double));
    int i, j;
    if (!gcol) return;
    p->base->grad(x, n, grad, p->base->ctx);
    if (p->m_ineq > 0 && p->ineq) {
        double *g = (double *)malloc((size_t)p->m_ineq * sizeof(double));
        if (g) {
            p->ineq(x, n, g, p->m_ineq, p->ineq_ctx);
            for (i = 0; i < p->m_ineq; ++i) {
                double coef = 0.0;
                if (p->sigma > 0 && p->lam) {
                    double t = p->lam[i] + p->sigma * g[i];
                    coef = t > 0 ? t : 0.0;
                } else {
                    coef = 2.0 * p->mu * (g[i] > 0 ? g[i] : 0.0);
                }
                if (coef != 0.0) {
                    ineq_grad_fd(p, i, x, n, gcol);
                    for (j = 0; j < n; ++j) grad[j] += coef * gcol[j];
                }
            }
            free(g);
        }
    }
    if (p->m_eq > 0 && p->eq) {
        double *h = (double *)malloc((size_t)p->m_eq * sizeof(double));
        if (h) {
            double mu = p->sigma > 0 ? p->sigma : p->mu;
            p->eq(x, n, h, p->m_eq, p->eq_ctx);
            for (i = 0; i < p->m_eq; ++i) {
                double coef;
                if (p->sigma > 0 && p->nu)
                    coef = p->nu[i] + p->sigma * h[i];
                else
                    coef = 2.0 * mu * h[i];
                if (coef != 0.0) {
                    eq_grad_fd(p, i, x, n, gcol);
                    for (j = 0; j < n; ++j) grad[j] += coef * gcol[j];
                }
            }
            free(h);
        }
    }
    free(gcol);
}

/*
 * 真 KKT 残差（路线图 L288：KKT 残差 < 1e-6 的可测量实现）：
 *   ‖∇f + Σ λ_i ∇g_i + Σ ν_j ∇h_j‖ + Σ max(0, g_i) + Σ |h_j|
 * 乘子来源：
 *   mu > 0  → 纯罚：λ_i = 2μ·max(0,g_i)，ν_j = 2μ·h_j（罚函数的乘子估计）
 *   mu <= 0 → 使用调用方提供的 lam/nu（增广拉格朗日的乘子）
 */
static double kkt_measure(const mlab_objective *obj,
                          mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                          mlab_eq_fn eq, int m_eq, void *eq_ctx,
                          const double *x, int n,
                          const double *lam, const double *nu, double mu)
{
    pen_ctx tmp;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *stat = (double *)malloc((size_t)n * sizeof(double));
    double *gcol = (double *)malloc((size_t)n * sizeof(double));
    double res = 0.0;
    int i, j;
    if (!g || !stat || !gcol) {
        free(g); free(stat); free(gcol);
        return 1e9;
    }
    tmp.base = obj;
    tmp.ineq = ineq;
    tmp.m_ineq = m_ineq;
    tmp.ineq_ctx = ineq_ctx;
    tmp.eq = eq;
    tmp.m_eq = m_eq;
    tmp.eq_ctx = eq_ctx;
    tmp.mu = mu;
    tmp.sigma = 0.0;
    tmp.lam = NULL;
    tmp.nu = NULL;
    obj->grad(x, n, stat, obj->ctx);
    if (ineq && m_ineq > 0) {
        double *gi = (double *)malloc((size_t)m_ineq * sizeof(double));
        if (gi) {
            ineq(x, n, gi, m_ineq, ineq_ctx);
            for (i = 0; i < m_ineq; ++i) {
                double li = (mu > 0) ? 2.0 * mu * (gi[i] > 0 ? gi[i] : 0.0)
                                     : (lam ? lam[i] : 0.0);
                if (li != 0.0) {
                    ineq_grad_fd(&tmp, i, x, n, gcol);
                    for (j = 0; j < n; ++j) stat[j] += li * gcol[j];
                }
                res += (gi[i] > 0) ? gi[i] : 0.0; /* 可行性违反 */
            }
            free(gi);
        }
    }
    if (eq && m_eq > 0) {
        double *hi = (double *)malloc((size_t)m_eq * sizeof(double));
        if (hi) {
            eq(x, n, hi, m_eq, eq_ctx);
            for (i = 0; i < m_eq; ++i) {
                double nui = (mu > 0) ? 2.0 * mu * hi[i] : (nu ? nu[i] : 0.0);
                if (nui != 0.0) {
                    eq_grad_fd(&tmp, i, x, n, gcol);
                    for (j = 0; j < n; ++j) stat[j] += nui * gcol[j];
                }
                res += fabs(hi[i]);
            }
            free(hi);
        }
    }
    res += mlab_nrm2(stat, n);
    free(g); free(stat); free(gcol);
    return res;
}

int mlab_penalty_outer(const mlab_objective *obj,
                       mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                       mlab_eq_fn eq, int m_eq, void *eq_ctx,
                       double mu, double *x, int max_iter, double tol,
                       double *kkt_res_out)
{
    pen_ctx pc;
    mlab_objective pobj;
    mlab_opt_run run;
    int n = obj->dim;
    double mu_cur = mu > 0 ? mu : 1.0;
    int outer;
    double *xsol = (double *)malloc((size_t)n * sizeof(double));

    if (!xsol) return -1;
    memcpy(xsol, x, (size_t)n * sizeof(double));
    pc.base = obj;
    pc.ineq = ineq;
    pc.m_ineq = m_ineq;
    pc.ineq_ctx = ineq_ctx;
    pc.eq = eq;
    pc.m_eq = m_eq;
    pc.eq_ctx = eq_ctx;
    pc.mu = mu_cur;
    pc.sigma = 0.0;
    pc.lam = NULL;
    pc.nu = NULL;
    pobj.dim = n;
    pobj.f = pen_f;
    pobj.grad = pen_g;
    pobj.hess = NULL;
    pobj.ctx = &pc;
    for (outer = 0; outer < max_iter; ++outer) {
        mlab_opt_run_init(&run, NULL, 0);
        mlab_opt_bfgs(&pobj, xsol, tol, 100, &run);
        {
            double kkt = kkt_measure(obj, ineq, m_ineq, ineq_ctx, eq, m_eq, eq_ctx,
                                     xsol, n, NULL, NULL, mu_cur);
            if (kkt_res_out) *kkt_res_out = kkt;
            if (kkt < tol) break;
        }
        mu_cur *= 10.0;
        pc.mu = mu_cur;
        if (mu_cur > 1e12) break;
    }
    memcpy(x, xsol, (size_t)n * sizeof(double));
    free(xsol);
    return 0;
}

int mlab_aug_lag(const mlab_objective *obj,
                 mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                 mlab_eq_fn eq, int m_eq, void *eq_ctx,
                 double sigma0, double *x, double *lam, double *nu,
                 int max_iter, double tol, double *kkt_res_out)
{
    pen_ctx pc;
    mlab_objective pobj;
    mlab_opt_run run;
    int n = obj->dim;
    double sigma = sigma0 > 0 ? sigma0 : 1.0;
    int outer, i;
    double *xsol = (double *)malloc((size_t)n * sizeof(double));

    if (!xsol) return -1;
    memcpy(xsol, x, (size_t)n * sizeof(double));
    if (lam) for (i = 0; i < m_ineq; ++i) lam[i] = 0.0;
    if (nu) for (i = 0; i < m_eq; ++i) nu[i] = 0.0;
    pc.base = obj;
    pc.ineq = ineq;
    pc.m_ineq = m_ineq;
    pc.ineq_ctx = ineq_ctx;
    pc.eq = eq;
    pc.m_eq = m_eq;
    pc.eq_ctx = eq_ctx;
    pc.mu = 0.0;
    pc.sigma = sigma;
    pc.lam = lam;
    pc.nu = nu;
    pobj.dim = n;
    pobj.f = pen_f;
    pobj.grad = pen_g;
    pobj.hess = NULL;
    pobj.ctx = &pc;
    for (outer = 0; outer < max_iter; ++outer) {
        pc.sigma = sigma;
        mlab_opt_run_init(&run, NULL, 0);
        mlab_opt_bfgs(&pobj, xsol, tol, 80, &run);
        /* update multipliers */
        if (ineq && lam) {
            double *gi = (double *)malloc((size_t)m_ineq * sizeof(double));
            if (gi) {
                ineq(xsol, n, gi, m_ineq, ineq_ctx);
                for (i = 0; i < m_ineq; ++i) {
                    double t = lam[i] + sigma * gi[i];
                    lam[i] = t < 0 ? 0 : t;
                }
                free(gi);
            }
        }
        if (eq && nu) {
            double *hi = (double *)malloc((size_t)m_eq * sizeof(double));
            if (hi) {
                eq(xsol, n, hi, m_eq, eq_ctx);
                for (i = 0; i < m_eq; ++i) nu[i] += sigma * hi[i];
                free(hi);
            }
        }
        {
            double kkt = kkt_measure(obj, ineq, m_ineq, ineq_ctx, eq, m_eq, eq_ctx,
                                     xsol, n, lam, nu, 0.0);
            if (kkt_res_out) *kkt_res_out = kkt;
            if (kkt < tol) break;
        }
        sigma *= 4.0;
        if (sigma > 1e10) sigma = 1e10;
    }
    memcpy(x, xsol, (size_t)n * sizeof(double));
    free(xsol);
    return 0;
}

/* ---- 投影梯度（投影 Armijo 回溯，Bertsekas 口径） ---- */

static void proj_box(int n, const double *lb, const double *ub, double *x)
{
    int i;
    for (i = 0; i < n; ++i) {
        if (x[i] < lb[i]) x[i] = lb[i];
        if (x[i] > ub[i]) x[i] = ub[i];
    }
}

int mlab_proj_grad_box(const mlab_objective *obj, const double *lb, const double *ub,
                       double *x, int max_iter, double tol, double *kkt_res_out)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    int it, i;
    if (!g || !xt) {
        free(g); free(xt);
        return -1;
    }
    for (it = 0; it < max_iter; ++it) {
        double f0, pgn = 0.0, alpha;
        obj->grad(x, n, g, obj->ctx);
        /* 投影梯度（驻点度量）：边界上指向盒外的分量清零 */
        {
            double *pg = (double *)malloc((size_t)n * sizeof(double));
            if (pg) {
                for (i = 0; i < n; ++i) {
                    pg[i] = g[i];
                    if ((x[i] <= lb[i] + 1e-12 && g[i] > 0) ||
                        (x[i] >= ub[i] - 1e-12 && g[i] < 0))
                        pg[i] = 0.0;
                }
                pgn = mlab_nrm2(pg, n);
                free(pg);
            }
        }
        if (kkt_res_out) *kkt_res_out = pgn;
        if (pgn < tol) break;
        f0 = obj->f(x, n, obj->ctx);
        alpha = 1.0;
        {
            int k, tried;
            for (tried = 0; tried < 60; ++tried) {
                double gd = 0.0, ft;
                for (k = 0; k < n; ++k) xt[k] = x[k] - alpha * g[k];
                proj_box(n, lb, ub, xt);
                for (k = 0; k < n; ++k) gd += g[k] * (x[k] - xt[k]);
                ft = obj->f(xt, n, obj->ctx);
                if (gd > 0.0 && ft <= f0 - 1e-4 * gd) break;
                alpha *= 0.5;
            }
        }
        for (i = 0; i < n; ++i) x[i] = xt[i];
    }
    free(g); free(xt);
    return 0;
}

/* ---- 对数障碍内点法（路线图 L277） ---- */

typedef struct {
    const mlab_objective *base;
    mlab_ineq_fn ineq;
    int m;
    void *ctx;
    double mu;
} barr_ctx;

static double barr_f(const double *x, int n, void *ctx)
{
    barr_ctx *p = (barr_ctx *)ctx;
    double *g = (double *)malloc((size_t)p->m * sizeof(double));
    double s = p->base->f(x, n, p->base->ctx);
    int i;
    if (!g) return 1e300;
    p->ineq(x, n, g, p->m, p->ctx);
    for (i = 0; i < p->m; ++i) {
        if (g[i] >= 0) {
            free(g);
            return 1e300; /* 离开可行域 */
        }
        s -= p->mu * log(-g[i]);
    }
    free(g);
    return s;
}

static void barr_g(const double *x, int n, double *grad, void *ctx)
{
    barr_ctx *p = (barr_ctx *)ctx;
    double *g = (double *)malloc((size_t)p->m * sizeof(double));
    double *gcol = (double *)malloc((size_t)n * sizeof(double));
    pen_ctx tmp;
    int i, j;
    if (!g || !gcol) {
        free(g); free(gcol);
        return;
    }
    p->base->grad(x, n, grad, p->base->ctx);
    p->ineq(x, n, g, p->m, p->ctx);
    tmp.base = p->base;
    tmp.ineq = p->ineq;
    tmp.m_ineq = p->m;
    tmp.ineq_ctx = p->ctx;
    tmp.eq = NULL;
    tmp.m_eq = 0;
    tmp.eq_ctx = NULL;
    tmp.mu = 0.0;
    tmp.sigma = 0.0;
    tmp.lam = NULL;
    tmp.nu = NULL;
    for (i = 0; i < p->m; ++i) {
        double coef = -p->mu / g[i]; /* d/dx [−μ ln(−g)] = μ g'/g（g<0） */
        ineq_grad_fd(&tmp, i, x, n, gcol);
        for (j = 0; j < n; ++j) grad[j] += coef * gcol[j];
    }
    free(g); free(gcol);
}

int mlab_log_barrier(const mlab_objective *obj,
                     mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                     double mu0, double *x, int max_outer, double tol,
                     double *kkt_res_out)
{
    barr_ctx bc;
    mlab_objective bobj;
    mlab_opt_run run;
    int n = obj->dim;
    double mu = mu0 > 0 ? mu0 : 1.0;
    int outer, i;
    double *xsol = (double *)malloc((size_t)n * sizeof(double));
    double *gi = (double *)malloc((size_t)m_ineq * sizeof(double));

    if (!xsol || !gi) {
        free(xsol); free(gi);
        return -1;
    }
    memcpy(xsol, x, (size_t)n * sizeof(double));
    ineq(xsol, n, gi, m_ineq, ineq_ctx);
    for (i = 0; i < m_ineq; ++i)
        if (gi[i] >= 0) {
            free(xsol); free(gi);
            return -2; /* 需要严格可行初值 */
        }
    bc.base = obj;
    bc.ineq = ineq;
    bc.m = m_ineq;
    bc.ctx = ineq_ctx;
    bc.mu = mu;
    bobj.dim = n;
    bobj.f = barr_f;
    bobj.grad = barr_g;
    bobj.hess = NULL;
    bobj.ctx = &bc;
    for (outer = 0; outer < max_outer; ++outer) {
        bc.mu = mu;
        mlab_opt_run_init(&run, NULL, 0);
        mlab_opt_bfgs(&bobj, xsol, tol, 120, &run);
        {
            /* 障碍乘子估计 λ_i = μ/(−g_i)，KKT = ‖∇f − Σλ∇g‖ + Σ|λ g_i + μ| */
            double *lam = (double *)malloc((size_t)m_ineq * sizeof(double));
            double kkt = 0.0;
            if (!lam) break;
            ineq(xsol, n, gi, m_ineq, ineq_ctx);
            for (i = 0; i < m_ineq; ++i) lam[i] = mu / (-gi[i]);
            {
                pen_ctx tmp;
                double *stat = (double *)malloc((size_t)n * sizeof(double));
                double *gcol = (double *)malloc((size_t)n * sizeof(double));
                tmp.base = obj;
                tmp.ineq = ineq;
                tmp.m_ineq = m_ineq;
                tmp.ineq_ctx = ineq_ctx;
                tmp.eq = NULL;
                tmp.m_eq = 0;
                tmp.eq_ctx = NULL;
                tmp.mu = 0.0;
                tmp.sigma = 0.0;
                tmp.lam = NULL;
                tmp.nu = NULL;
                if (stat && gcol) {
                    obj->grad(xsol, n, stat, obj->ctx);
                    for (i = 0; i < m_ineq; ++i) {
                        int j;
                        ineq_grad_fd(&tmp, i, xsol, n, gcol);
                        for (j = 0; j < n; ++j) stat[j] += lam[i] * gcol[j]; /* KKT: grad f + sum lam*g_grad = 0 */
                        kkt += fabs(lam[i] * (-gi[i])); /* 原问题互补松弛 |λ(-g)|：随 μ->0 收敛 */
                    }
                    kkt += mlab_nrm2(stat, n);
                }
                free(stat);
                free(gcol);
            }
            free(lam);
            if (kkt_res_out) *kkt_res_out = kkt;
            if (kkt < tol) break;
        }
        mu *= 0.1;
        if (mu <= 1e-6) break; /* μ 过小时 λ=μ/(-g) 的数值估计失真：在 μ~1e-6 处停，
                                  KKT 残差 ~O(μ) 量级即达判据 */
    }
    memcpy(x, xsol, (size_t)n * sizeof(double));
    free(xsol);
    free(gi);
    return 0;
}

/* ---- 最小 SQP（有效集 QP 子问题 + merit 回溯，路线图 L279 概览） ---- */

static void fd_ineq_col(mlab_ineq_fn ineq, int idx, const double *x, int n,
                        int m, void *ctx, double *gcol)
{
    int k;
    double *xp = (double *)malloc((size_t)n * sizeof(double));
    double *gp = (double *)malloc((size_t)m * sizeof(double));
    double *gm = (double *)malloc((size_t)m * sizeof(double));
    if (!xp || !gp || !gm) {
        free(xp); free(gp); free(gm);
        return;
    }
    for (k = 0; k < n; ++k) {
        double hs = 1e-6 * (fabs(x[k]) > 1 ? fabs(x[k]) : 1);
        memcpy(xp, x, (size_t)n * sizeof(double));
        xp[k] += hs;
        ineq(xp, n, gp, m, ctx);
        xp[k] = x[k] - hs;
        ineq(xp, n, gm, m, ctx);
        gcol[k] = (gp[idx] - gm[idx]) / (2.0 * hs);
    }
    free(xp); free(gp); free(gm);
}

static void fd_eq_col(mlab_eq_fn eq, int idx, const double *x, int n,
                      int m, void *ctx, double *gcol)
{
    int k;
    double *xp = (double *)malloc((size_t)n * sizeof(double));
    double *hp = (double *)malloc((size_t)m * sizeof(double));
    double *hm = (double *)malloc((size_t)m * sizeof(double));
    if (!xp || !hp || !hm) {
        free(xp); free(hp); free(hm);
        return;
    }
    for (k = 0; k < n; ++k) {
        double hs = 1e-6 * (fabs(x[k]) > 1 ? fabs(x[k]) : 1);
        memcpy(xp, x, (size_t)n * sizeof(double));
        xp[k] += hs;
        eq(xp, n, hp, m, ctx);
        xp[k] = x[k] - hs;
        eq(xp, n, hm, m, ctx);
        gcol[k] = (hp[idx] - hm[idx]) / (2.0 * hs);
    }
    free(xp); free(hp); free(hm);
}

int mlab_sqp_min(const mlab_objective *obj,
                 mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                 mlab_eq_fn eq, int m_eq, void *eq_ctx,
                 double *x, int max_iter, double tol, double *kkt_res_out)
{
    int n = obj->dim;
    int it, i, j;
    int mact_max = m_ineq + m_eq;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *K = (double *)malloc((size_t)(n + mact_max) * (n + mact_max) * sizeof(double));
    double *rhs = (double *)malloc((size_t)(n + mact_max) * sizeof(double));
    double *sol = (double *)malloc((size_t)(n + mact_max) * sizeof(double));
    double *gi = (double *)malloc((size_t)(m_ineq > 0 ? m_ineq : 1) * sizeof(double));
    double *hi = (double *)malloc((size_t)(m_eq > 0 ? m_eq : 1) * sizeof(double));
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    double *stat = (double *)malloc((size_t)n * sizeof(double));
    double *gcol = (double *)malloc((size_t)n * sizeof(double));
    double *lam = (double *)calloc((size_t)(m_ineq > 0 ? m_ineq : 1), sizeof(double));
    double *nu = (double *)calloc((size_t)(m_eq > 0 ? m_eq : 1), sizeof(double));

    if (!g || !K || !rhs || !sol || !gi || !hi || !xt || !stat || !gcol || !lam || !nu) {
        free(g); free(K); free(rhs); free(sol); free(gi); free(hi); free(xt);
        free(stat); free(gcol); free(lam); free(nu);
        return -1;
    }
    for (it = 0; it < max_iter; ++it) {
        int mact = 0, rc;
        double kkt, f0, phi0, phi_t, t;
        obj->grad(x, n, g, obj->ctx);
        if (ineq) ineq(x, n, gi, m_ineq, ineq_ctx);
        if (eq) eq(x, n, hi, m_eq, eq_ctx);
        /* KKT 残差（含上一步 QP 的乘子估计）：
           ‖∇f + Σλ∇g + Σν∇h‖ + Σ max(0,g) + Σ|h| */
        for (i = 0; i < n; ++i) stat[i] = g[i];
        for (i = 0; i < m_ineq; ++i) {
            if (lam[i] == 0.0) continue;
            fd_ineq_col(ineq, i, x, n, m_ineq, ineq_ctx, gcol);
            for (j = 0; j < n; ++j) stat[j] += lam[i] * gcol[j];
        }
        for (i = 0; i < m_eq; ++i) {
            if (nu[i] == 0.0) continue;
            fd_eq_col(eq, i, x, n, m_eq, eq_ctx, gcol);
            for (j = 0; j < n; ++j) stat[j] += nu[i] * gcol[j];
        }
        kkt = mlab_nrm2(stat, n);
        for (i = 0; i < m_ineq; ++i) kkt += gi[i] > 0 ? gi[i] : 0.0;
        for (i = 0; i < m_eq; ++i) kkt += fabs(hi[i]);
        if (kkt_res_out) *kkt_res_out = kkt;
        if (kkt < tol) break;
        /* 组装 QP 子问题（H=I）：
           min 0.5 d'd + ∇f'd  s.t. ∇g_i·d <= -g_i（激活）, ∇h·d = -h */
        for (i = 0; i < (n + mact_max) * (n + mact_max); ++i) K[i] = 0.0;
        for (i = 0; i < n + mact_max; ++i) rhs[i] = 0.0;
        for (i = 0; i < n; ++i) {
            for (j = 0; j < n; ++j) K[i * (n + mact_max) + j] = (i == j) ? 1.0 : 0.0;
            rhs[i] = -g[i];
        }
        for (i = 0; i < m_ineq; ++i) {
            int row, k;
            if (gi[i] < -1e-10) continue; /* 非激活 */
            row = n + mact;
            fd_ineq_col(ineq, i, x, n, m_ineq, ineq_ctx, gcol);
            for (k = 0; k < n; ++k) {
                K[row * (n + mact_max) + k] = gcol[k];
                K[k * (n + mact_max) + row] = gcol[k]; /* 转置块 G^T */
            }
            rhs[row] = -gi[i];
            ++mact;
        }
        for (i = 0; i < m_eq; ++i) {
            int row, k;
            row = n + mact;
            fd_eq_col(eq, i, x, n, m_eq, eq_ctx, gcol);
            for (k = 0; k < n; ++k) {
                K[row * (n + mact_max) + k] = gcol[k];
                K[k * (n + mact_max) + row] = gcol[k]; /* 转置块 G^T */
            }
            rhs[row] = -hi[i];
            ++mact;
        }
        if (mact == 0) {
            for (i = 0; i < n; ++i) x[i] -= 0.5 * g[i];
            continue;
        }
        rc = mlab_lu_solve_dense(K, n + mact, rhs, sol);
        if (rc != 0) break;
        /* 更新乘子估计（来自 QP） */
        for (i = 0; i < m_ineq; ++i) lam[i] = 0.0;
        {
            int row = n;
            for (i = 0; i < m_ineq; ++i) {
                if (gi[i] < -1e-10) continue;
                lam[i] = sol[row];
                ++row;
            }
            for (i = 0; i < m_eq; ++i) {
                nu[i] = sol[row];
                ++row;
            }
        }
        /* merit 回溯：φ = f + 10·viol */
        f0 = obj->f(x, n, obj->ctx);
        phi0 = f0;
        for (i = 0; i < m_ineq; ++i) phi0 += 10.0 * (gi[i] > 0 ? gi[i] : 0.0);
        for (i = 0; i < m_eq; ++i) phi0 += 10.0 * fabs(hi[i]);
        t = 1.0;
        for (i = 0; i < 30; ++i) {
            for (j = 0; j < n; ++j) xt[j] = x[j] + t * sol[j];
            phi_t = obj->f(xt, n, obj->ctx);
            if (ineq) {
                ineq(xt, n, gi, m_ineq, ineq_ctx);
                for (j = 0; j < m_ineq; ++j) phi_t += 10.0 * (gi[j] > 0 ? gi[j] : 0.0);
            }
            if (eq) {
                eq(xt, n, hi, m_eq, eq_ctx);
                for (j = 0; j < m_eq; ++j) phi_t += 10.0 * fabs(hi[j]);
            }
            if (phi_t < phi0) break;
            t *= 0.5;
        }
        for (j = 0; j < n; ++j) x[j] += t * sol[j];
    }
    free(g); free(K); free(rhs); free(sol); free(gi); free(hi); free(xt);
    free(stat); free(gcol); free(lam); free(nu);
    return 0;
}
