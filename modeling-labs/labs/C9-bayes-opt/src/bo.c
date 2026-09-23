#include "bo.h"
#include "optcore.h"
#include "opt.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_bo_result_free(mlab_bo_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->best_hist);
    r->best_x = NULL;
    r->best_hist = NULL;
}

void mlab_lhs(mlab_rng *rng, int n, int dim,
              const double *lb, const double *ub, double *X)
{
    int i, d;
    if (!rng || n < 1 || dim < 1 || !X || !lb || !ub) return;
    /* 每维独立分桶 + 桶内抖动 + 桶序随机置换（标准 LHS） */
    for (d = 0; d < dim; ++d) {
        int *perm = (int *)malloc((size_t)n * sizeof(int));
        if (!perm) continue;
        for (i = 0; i < n; ++i) perm[i] = i;
        for (i = n - 1; i > 0; --i) {
            int k = (int)(mlab_rng_uniform(rng) * (i + 1));
            int t;
            if (k < 0) k = 0;
            if (k > i) k = i;
            t = perm[i]; perm[i] = perm[k]; perm[k] = t;
        }
        for (i = 0; i < n; ++i) {
            double u = ((double)perm[i] + mlab_rng_uniform(rng)) / (double)n;
            X[i * dim + d] = lb[d] + (ub[d] - lb[d]) * u;
        }
        free(perm);
    }
}

typedef struct {
    const mlab_gp *gp;
    mlab_bo_acq acq;
    double best, xi, kappa;
    int dim;
    const double *lb, *ub;
} acq_ctx;

static double acq_value(const acq_ctx *c, const double *x)
{
    double mean, var, v;
    if (!mlab_gp_predict(c->gp, x, &mean, &var)) return -1e300;
    if (c->acq == MLAB_BO_ACQ_PI)
        v = mlab_acq_pi(mean, var, c->best, c->xi);
    else if (c->acq == MLAB_BO_ACQ_UCB)
        v = mlab_acq_ucb(mean, var, c->kappa > 0 ? c->kappa : 2.0);
    else
        v = mlab_acq_ei(mean, var, c->best, c->xi);
    return v;
}

static double neg_acq(const double *x, int n, void *ctx)
{
    const acq_ctx *c = (const acq_ctx *)ctx;
    (void)n;
    return -acq_value(c, x);
}

static void neg_acq_grad(const double *x, int n, double *g, void *ctx)
{
    const acq_ctx *c = (const acq_ctx *)ctx;
    int d;
    double h = 1e-5;
    for (d = 0; d < n; ++d) {
        double xp[16], xm[16];
        memcpy(xp, x, (size_t)n * sizeof(double));
        memcpy(xm, x, (size_t)n * sizeof(double));
        xp[d] += h;
        xm[d] -= h;
        if (c->lb && xp[d] < c->lb[d]) xp[d] = c->lb[d];
        if (c->ub && xp[d] > c->ub[d]) xp[d] = c->ub[d];
        if (c->lb && xm[d] < c->lb[d]) xm[d] = c->lb[d];
        if (c->ub && xm[d] > c->ub[d]) xm[d] = c->ub[d];
        g[d] = (neg_acq(xp, n, ctx) - neg_acq(xm, n, ctx)) / (xp[d] - xm[d] + 1e-16);
    }
}

static void clip_x(double *x, int dim, const double *lb, const double *ub)
{
    int d;
    if (!lb || !ub) return;
    for (d = 0; d < dim; ++d) {
        if (x[d] < lb[d]) x[d] = lb[d];
        if (x[d] > ub[d]) x[d] = ub[d];
    }
}

double mlab_bo_maximize_acq(const mlab_gp *gp, mlab_bo_acq acq,
                            double best, double xi, double kappa,
                            const double *lb, const double *ub, int dim,
                            int grid_n, int use_bfgs, int bfgs_starts,
                            double *x_out)
{
    acq_ctx c;
    double *grid_x, best_v = -1e300;
    int i, d, ngrid, g0, gi;
    int *order;
    double *vals;

    if (!gp || !gp->fitted || dim < 1 || dim > MLAB_BO_MAX_DIM || !lb || !ub)
        return 0.0;
    c.gp = gp;
    c.acq = acq;
    c.best = best;
    c.xi = xi;
    c.kappa = kappa;
    c.dim = dim;
    c.lb = lb;
    c.ub = ub;
    if (grid_n < 5) grid_n = 15;
    if (dim > 8) grid_n = 8; /* 防网格爆炸 */

    /* 粗网格：每维 grid_n 点（dim 小时全网格；dim 大时用随机网格） */
    if (dim <= 2) {
        ngrid = 1;
        for (d = 0; d < dim; ++d) ngrid *= grid_n;
        if (ngrid > 20000) ngrid = 20000;
        grid_x = (double *)malloc((size_t)ngrid * dim * sizeof(double));
        vals = (double *)malloc((size_t)ngrid * sizeof(double));
        order = (int *)malloc((size_t)ngrid * sizeof(int));
        if (!grid_x || !vals || !order) {
            free(grid_x); free(vals); free(order);
            return 0.0;
        }
        for (gi = 0; gi < ngrid; ++gi) {
            int t = gi;
            for (d = 0; d < dim; ++d) {
                int idx = t % grid_n;
                t /= grid_n;
                grid_x[gi * dim + d] = lb[d] +
                    (ub[d] - lb[d]) * ((double)idx / (double)(grid_n - 1));
            }
            vals[gi] = acq_value(&c, &grid_x[gi * dim]);
            order[gi] = gi;
            if (gi == 0 && x_out)
                memcpy(x_out, &grid_x[0], (size_t)dim * sizeof(double));
            if (vals[gi] > best_v) {
                best_v = vals[gi];
                if (x_out) memcpy(x_out, &grid_x[gi * dim], (size_t)dim * sizeof(double));
            }
        }
    } else {
        ngrid = grid_n * 8;
        grid_x = (double *)malloc((size_t)ngrid * dim * sizeof(double));
        vals = (double *)malloc((size_t)ngrid * sizeof(double));
        order = (int *)malloc((size_t)ngrid * sizeof(int));
        if (!grid_x || !vals || !order) {
            free(grid_x); free(vals); free(order);
            return 0.0;
        }
        /* 均匀撒点（确定性网格的稀疏版） */
        for (gi = 0; gi < ngrid; ++gi) {
            for (d = 0; d < dim; ++d) {
                double u = fmod((double)((gi + 1) * (d + 3) * 2654435761u) / 4294967296.0, 1.0);
                grid_x[gi * dim + d] = lb[d] + (ub[d] - lb[d]) * u;
            }
            vals[gi] = acq_value(&c, &grid_x[gi * dim]);
            order[gi] = gi;
            if (gi == 0 && x_out)
                memcpy(x_out, &grid_x[0], (size_t)dim * sizeof(double));
            if (vals[gi] > best_v) {
                best_v = vals[gi];
                if (x_out) memcpy(x_out, &grid_x[gi * dim], (size_t)dim * sizeof(double));
            }
        }
    }

    /* 对 vals 降序排序 order（选择排序，ngrid 不大） */
    for (i = 0; i < ngrid && i < 32; ++i) {
        int k = i, t;
        for (g0 = i + 1; g0 < ngrid; ++g0)
            if (vals[order[g0]] > vals[order[k]]) k = g0;
        t = order[i]; order[i] = order[k]; order[k] = t;
    }

    if (use_bfgs && bfgs_starts > 0) {
        int nstart = bfgs_starts < 8 ? bfgs_starts : 8;
        if (nstart > ngrid) nstart = ngrid;
        for (i = 0; i < nstart; ++i) {
            double x[16];
            mlab_objective obj;
            mlab_opt_run run;
            double v;
            memcpy(x, &grid_x[order[i] * dim], (size_t)dim * sizeof(double));
            memset(&obj, 0, sizeof obj);
            obj.dim = dim;
            obj.f = neg_acq;
            obj.grad = neg_acq_grad;
            obj.ctx = &c;
            mlab_opt_run_init(&run, NULL, 0);
            mlab_opt_bfgs(&obj, x, 1e-5, 40, &run);
            clip_x(x, dim, lb, ub);
            v = acq_value(&c, x);
            if (v > best_v) {
                best_v = v;
                if (x_out) memcpy(x_out, x, (size_t)dim * sizeof(double));
            }
        }
    }

    free(grid_x);
    free(vals);
    free(order);
    return best_v;
}

static int result_init(mlab_bo_result *out, int dim, int max_evals)
{
    memset(out, 0, sizeof *out);
    out->best_f = 1e300;
    out->evals_to_target = -1;
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->best_hist = (double *)malloc((size_t)(max_evals + 2) * sizeof(double));
    return out->best_x && out->best_hist;
}

static void consider(mlab_bo_result *out, const double *x, double fx,
                     int dim, double target, int n_eval)
{
    int i;
    if (fx < out->best_f) {
        out->best_f = fx;
        for (i = 0; i < dim; ++i) out->best_x[i] = x[i];
    }
    if (out->hist_len >= 0 && out->best_hist)
        out->best_hist[out->hist_len++] = out->best_f;
    if (out->evals_to_target < 0 && target > -1e299 && out->best_f <= target) {
        out->reached = 1;
        out->evals_to_target = n_eval;
    }
}

/* bo_run / bo_run_batch 共用的预算与缓冲初始化 */
static int bo_alloc(const mlab_bo_config *cfg, mlab_bo_result *out,
                    int *dim_out, int *n_init_out, int *maxe_out,
                    double **X_out, double **y_out)
{
    double *X, *y;
    int dim, n_init, maxe;

    if (!cfg || !cfg->f || !out || cfg->dim < 1 ||
        cfg->dim > MLAB_BO_MAX_DIM) return 0;
    dim = cfg->dim;
    n_init = cfg->n_init > 2 ? cfg->n_init : 8;
    maxe = cfg->max_evals > n_init ? cfg->max_evals : n_init + 20;
    if (n_init >= maxe) n_init = maxe / 2;
    if (!result_init(out, dim, maxe)) {
        mlab_bo_result_free(out);
        return 0;
    }
    X = (double *)malloc((size_t)maxe * dim * sizeof(double));
    y = (double *)malloc((size_t)maxe * sizeof(double));
    if (!X || !y) {
        free(X);
        free(y);
        mlab_bo_result_free(out);
        return 0;
    }
    *dim_out = dim;
    *n_init_out = n_init;
    *maxe_out = maxe;
    *X_out = X;
    *y_out = y;
    return 1;
}

/* 配置边界逐维兜底 [0,1]（原实现仅 dim=1 可兜底，现显式逐维） */
static void bo_bounds(const mlab_bo_config *cfg, double *lb_b, double *ub_b)
{
    int d;
    for (d = 0; d < cfg->dim; ++d) {
        lb_b[d] = cfg->lb ? cfg->lb[d] : 0.0;
        ub_b[d] = cfg->ub ? cfg->ub[d] : 1.0;
    }
}

/* LHS 初始设计 + 逐点真评估（含 consider）；X/y 由调用方分配 */
static void bo_seed_lhs(mlab_rng *rng, const mlab_bo_config *cfg,
                        mlab_bo_result *out, double *X, double *y,
                        int n_init, double target)
{
    int i, dim = cfg->dim;
    double *X0 = (double *)malloc((size_t)n_init * dim * sizeof(double));
    if (!X0) {
        /* 兜底：均匀随机撒初始点 */
        int d;
        for (i = 0; i < n_init; ++i) {
            double fx, x[MLAB_BO_MAX_DIM];
            for (d = 0; d < dim; ++d) {
                double lo = cfg->lb ? cfg->lb[d] : 0.0;
                double hi = cfg->ub ? cfg->ub[d] : 1.0;
                x[d] = lo + (hi - lo) * mlab_rng_uniform(rng);
            }
            fx = cfg->f(x, dim, cfg->ctx);
            memcpy(&X[i * dim], x, (size_t)dim * sizeof(double));
            y[i] = fx;
            ++out->n_eval;
            consider(out, x, fx, dim, target, out->n_eval);
        }
        return;
    }
    mlab_lhs(rng, n_init, dim, cfg->lb, cfg->ub, X0);
    for (i = 0; i < n_init; ++i) {
        double fx = cfg->f(&X0[i * dim], dim, cfg->ctx);
        memcpy(&X[i * dim], &X0[i * dim], (size_t)dim * sizeof(double));
        y[i] = fx;
        ++out->n_eval;
        consider(out, &X[i * dim], fx, dim, target, out->n_eval);
    }
    free(X0);
}

int mlab_bo_run(mlab_rng *rng, const mlab_bo_config *cfg,
                double target, mlab_bo_result *out)
{
    double *X, *y;
    int dim, n_init, maxe, d, n;
    double lb_b[MLAB_BO_MAX_DIM], ub_b[MLAB_BO_MAX_DIM];

    if (!rng || !bo_alloc(cfg, out, &dim, &n_init, &maxe, &X, &y)) return 0;
    bo_bounds(cfg, lb_b, ub_b);

    /* LHS 初始设计 */
    bo_seed_lhs(rng, cfg, out, X, y, n_init, target);
    n = out->n_eval;

    while (out->n_eval < maxe) {
        mlab_gp gp;
        double xnext[MLAB_BO_MAX_DIM];
        double fx, best_y = out->best_f;
        mlab_gp_init(&gp, dim, X, y, n,
                     cfg->ls > 0 ? cfg->ls : 0.25,
                     cfg->sf2 > 0 ? cfg->sf2 : 1.0,
                     cfg->noise > 0 ? cfg->noise : 1e-4);
        /* 兜底初值：盒中心（采集最大化失败/未写 x_out 时仍有确定候选） */
        for (d = 0; d < dim; ++d)
            xnext[d] = 0.5 * (lb_b[d] + ub_b[d]);
        if (!mlab_gp_fit(&gp)) {
            /* GP 失败则随机补点 */
            for (d = 0; d < dim; ++d)
                xnext[d] = lb_b[d] + (ub_b[d] - lb_b[d]) * mlab_rng_uniform(rng);
        } else {
            mlab_bo_maximize_acq(&gp, cfg->acq, best_y,
                                 cfg->xi, cfg->kappa,
                                 lb_b, ub_b, dim,
                                 cfg->grid_n > 0 ? cfg->grid_n : 15,
                                 cfg->use_bfgs,
                                 cfg->bfgs_starts > 0 ? cfg->bfgs_starts : 4,
                                 xnext);
            clip_x(xnext, dim, lb_b, ub_b);
        }
        mlab_gp_release(&gp);
        fx = cfg->f(xnext, dim, cfg->ctx);
        memcpy(&X[n * dim], xnext, (size_t)dim * sizeof(double));
        y[n] = fx;
        ++n;
        ++out->n_eval;
        consider(out, xnext, fx, dim, target, out->n_eval);
        if (out->reached) break;
    }
    free(X);
    free(y);
    return 1;
}

int mlab_bo_run_batch(mlab_rng *rng, const mlab_bo_config *cfg, int q,
                      double target, mlab_bo_result *out)
{
    double *X, *y, *pend_fx;
    int dim, n_init, maxe, n, b;
    double lb_b[MLAB_BO_MAX_DIM], ub_b[MLAB_BO_MAX_DIM];

    if (!rng || q < 1 || !bo_alloc(cfg, out, &dim, &n_init, &maxe, &X, &y))
        return 0;
    bo_bounds(cfg, lb_b, ub_b);
    pend_fx = (double *)malloc((size_t)(q > maxe ? q : maxe) * sizeof(double));
    if (!pend_fx) {
        free(X);
        free(y);
        mlab_bo_result_free(out);
        return 0;
    }

    bo_seed_lhs(rng, cfg, out, X, y, n_init, target);
    n = out->n_eval;

    while (out->n_eval < maxe) {
        int qn = q < (maxe - out->n_eval) ? q : (maxe - out->n_eval);
        double liar = out->best_f; /* 常量 liar = 批开始时的当前最优观测 */
        if (qn < 1) break;
        for (b = 0; b < qn; ++b) {
            mlab_gp gp;
            double xnext[MLAB_BO_MAX_DIM];
            double fx;
            int nb = n + b; /* 训练集 = 已评估 n 点 + b 个 liar pending 点 */
            int d, ok_fit;
            if (b > 0) {
                int t;
                for (t = 0; t < b; ++t) y[n + t] = liar;
            }
            mlab_gp_init(&gp, dim, X, y, nb,
                         cfg->ls > 0 ? cfg->ls : 0.25,
                         cfg->sf2 > 0 ? cfg->sf2 : 1.0,
                         cfg->noise > 0 ? cfg->noise : 1e-4);
            ok_fit = mlab_gp_fit(&gp);
            for (d = 0; d < dim; ++d)
                xnext[d] = 0.5 * (lb_b[d] + ub_b[d]);
            if (ok_fit) {
                mlab_bo_maximize_acq(&gp, cfg->acq, out->best_f,
                                     cfg->xi, cfg->kappa,
                                     lb_b, ub_b, dim,
                                     cfg->grid_n > 0 ? cfg->grid_n : 15,
                                     cfg->use_bfgs,
                                     cfg->bfgs_starts > 0 ? cfg->bfgs_starts : 4,
                                     xnext);
                clip_x(xnext, dim, lb_b, ub_b);
            } else {
                for (d = 0; d < dim; ++d)
                    xnext[d] = lb_b[d] + (ub_b[d] - lb_b[d]) * mlab_rng_uniform(rng);
            }
            mlab_gp_release(&gp);
            fx = cfg->f(xnext, dim, cfg->ctx);
            memcpy(&X[nb * dim], xnext, (size_t)dim * sizeof(double));
            pend_fx[b] = fx;
            y[nb] = liar; /* 批内后续成员仍用 liar；真实值批末写回 */
            ++out->n_eval;
        }
        /* 批末：真实观测写回并按批内顺序计入 running best */
        for (b = 0; b < qn; ++b) {
            y[n + b] = pend_fx[b];
            consider(out, &X[(n + b) * dim], pend_fx[b], dim, target,
                     out->n_eval - qn + b + 1);
            ++n;
            if (out->reached) break;
        }
        if (out->reached) break;
    }
    free(pend_fx);
    free(X);
    free(y);
    return 1;
}

int mlab_bo_random(mlab_rng *rng, const mlab_bo_config *cfg,
                   double target, mlab_bo_result *out)
{
    int dim, maxe, i, d;
    double x[MLAB_BO_MAX_DIM];
    if (!rng || !cfg || !cfg->f || !out || cfg->dim < 1 ||
        cfg->dim > MLAB_BO_MAX_DIM) return 0;
    dim = cfg->dim;
    maxe = cfg->max_evals > 0 ? cfg->max_evals : 50;
    if (!result_init(out, dim, maxe)) {
        mlab_bo_result_free(out);
        return 0;
    }
    for (i = 0; i < maxe; ++i) {
        double fx;
        for (d = 0; d < dim; ++d) {
            double lo = cfg->lb ? cfg->lb[d] : 0.0;
            double hi = cfg->ub ? cfg->ub[d] : 1.0;
            x[d] = lo + (hi - lo) * mlab_rng_uniform(rng);
        }
        fx = cfg->f(x, dim, cfg->ctx);
        ++out->n_eval;
        consider(out, x, fx, dim, target, out->n_eval);
        if (out->reached) break;
    }
    return 1;
}

int mlab_bo_lhs_baseline(mlab_rng *rng, const mlab_bo_config *cfg,
                         double target, mlab_bo_result *out)
{
    double *X;
    int dim, maxe, i;
    if (!rng || !cfg || !cfg->f || !out || cfg->dim < 1 ||
        cfg->dim > MLAB_BO_MAX_DIM) return 0;
    dim = cfg->dim;

    maxe = cfg->max_evals > 0 ? cfg->max_evals : 50;
    if (!result_init(out, dim, maxe)) {
        mlab_bo_result_free(out);
        return 0;
    }
    X = (double *)malloc((size_t)maxe * dim * sizeof(double));
    if (!X) { mlab_bo_result_free(out); return 0; }
    mlab_lhs(rng, maxe, dim, cfg->lb, cfg->ub, X);
    for (i = 0; i < maxe; ++i) {
        double fx = cfg->f(&X[i * dim], dim, cfg->ctx);
        ++out->n_eval;
        consider(out, &X[i * dim], fx, dim, target, out->n_eval);
        if (out->reached) break;
    }
    free(X);
    return 1;
}
