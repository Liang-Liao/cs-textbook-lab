#include "e2e.h"
#include "de.h"
#include "ode.h"
#include "opt.h"
#include "optcore.h"
#include "stats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- 配置与仿真 ---------- */

void mlab_e2e_default_lv(mlab_e2e_config *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof *cfg);
    cfg->model_id = MLAB_E2E_MODEL_LV;
    cfg->npar = MLAB_E2E_NPAR;
    cfg->theta_true[0] = 1.0;   /* alpha */
    cfg->theta_true[1] = 0.08;  /* beta  */
    cfg->theta_true[2] = 0.6;   /* gamma */
    cfg->theta_true[3] = 0.04;  /* delta */
    cfg->y0[0] = 12.0;
    cfg->y0[1] = 4.0;
    cfg->t0 = 0.0;
    cfg->t1 = 18.0;
    cfg->n_obs = 25;
    cfg->sigma = 0.7;
    cfg->h_ode = 0.05;
    cfg->lb[0] = 0.20; cfg->ub[0] = 3.00;
    cfg->lb[1] = 0.010; cfg->ub[1] = 0.40;
    cfg->lb[2] = 0.10; cfg->ub[2] = 2.00;
    cfg->lb[3] = 0.005; cfg->ub[3] = 0.20;
}

int mlab_e2e_lv_simulate(const double theta[MLAB_E2E_NPAR], const double y0[2],
                         const double *t_obs, int n_obs,
                         double h_ode, double *y_out)
{
    mlab_lv_ctx ctx;
    double y[2];
    double t;
    int i, k, m;
    if (!theta || !y0 || !t_obs || !y_out || n_obs <= 0 || n_obs > MLAB_E2E_MAX_OBS)
        return -1;
    if (h_ode <= 0.0) h_ode = 0.05;
    for (k = 0; k < MLAB_E2E_NPAR; ++k)
        if (!(theta[k] > 0.0) || !isfinite(theta[k])) return -2;

    ctx.alpha = theta[0];
    ctx.beta = theta[1];
    ctx.gamma = theta[2];
    ctx.delta = theta[3];
    y[0] = y0[0];
    y[1] = y0[1];
    t = t_obs[0];
    y_out[0] = y[0];
    y_out[1] = y[1];
    for (i = 1; i < n_obs; ++i) {
        double t_next = t_obs[i];
        if (t_next < t) return -2;
        if (t_next > t) {
            if (mlab_ode_rk4(mlab_lv_rhs, &ctx, 2, t, t_next, h_ode, y) != 0)
                return -2;
            t = t_next;
        }
        y_out[2 * i + 0] = y[0];
        y_out[2 * i + 1] = y[1];
    }
    m = 2 * n_obs;
    for (k = 0; k < m; ++k) {
        if (!isfinite(y_out[k]) || y_out[k] < -1.0 || y_out[k] > 1e6) return -2;
        if (y_out[k] < 0.0) y_out[k] = 0.0;
    }
    return 0;
}

int mlab_e2e_seir_simulate(const double theta[MLAB_E2E_NPAR], const double y0[2],
                           const double *t_obs, int n_obs,
                           double h_ode, double *y_out)
{
    /* 预留：SEIR 模板走 D2；观测只写 I 轨迹到 y_out 的 prey 槽，便于接口扩展 */
    mlab_seir_ctx ctx;
    double y[4];
    double t;
    int i, k;
    if (!theta || !y0 || !t_obs || !y_out || n_obs <= 0) return -1;
    if (h_ode <= 0.0) h_ode = 0.05;
    ctx.beta = theta[0];
    ctx.sigma = theta[1];
    ctx.gamma = theta[2];
    ctx.N = (theta[3] > 0) ? theta[3] : (y0[0] + y0[1] + 100.0);
    y[0] = ctx.N - y0[0] - y0[1];
    y[1] = y0[0];
    y[2] = y0[1];
    y[3] = 0.0;
    t = t_obs[0];
    y_out[0] = y[2];
    y_out[1] = y[0];
    for (i = 1; i < n_obs; ++i) {
        if (mlab_ode_rk4(mlab_seir_rhs, &ctx, 4, t, t_obs[i], h_ode, y) != 0)
            return -2;
        t = t_obs[i];
        y_out[2 * i + 0] = y[2];
        y_out[2 * i + 1] = y[0];
    }
    for (k = 0; k < 2 * n_obs; ++k) {
        if (!isfinite(y_out[k])) return -2;
        if (y_out[k] < 0.0) y_out[k] = 0.0;
    }
    return 0;
}

void mlab_e2e_obs_free(mlab_e2e_obs *obs)
{
    if (!obs) return;
    free(obs->t);
    free(obs->yobs);
    free(obs->yclean);
    obs->t = NULL;
    obs->yobs = NULL;
    obs->yclean = NULL;
    obs->n_obs = 0;
}

int mlab_e2e_synth(mlab_rng *rng, const mlab_e2e_config *cfg,
                   mlab_e2e_obs *obs)
{
    int i, n, m;
    if (!rng || !cfg || !obs || cfg->n_obs <= 0 || cfg->n_obs > MLAB_E2E_MAX_OBS)
        return -1;
    memset(obs, 0, sizeof *obs);
    n = cfg->n_obs;
    m = 2 * n;
    obs->t = (double *)malloc((size_t)n * sizeof(double));
    obs->yobs = (double *)malloc((size_t)m * sizeof(double));
    obs->yclean = (double *)malloc((size_t)m * sizeof(double));
    if (!obs->t || !obs->yobs || !obs->yclean) {
        mlab_e2e_obs_free(obs);
        return -1;
    }
    for (i = 0; i < n; ++i)
        obs->t[i] = cfg->t0 + (cfg->t1 - cfg->t0) * (double)i / (double)(n - 1);

    if (cfg->model_id == MLAB_E2E_MODEL_SEIR) {
        if (mlab_e2e_seir_simulate(cfg->theta_true, cfg->y0, obs->t, n,
                                   cfg->h_ode, obs->yclean) != 0) {
            mlab_e2e_obs_free(obs);
            return -1;
        }
    } else {
        if (mlab_e2e_lv_simulate(cfg->theta_true, cfg->y0, obs->t, n,
                                 cfg->h_ode, obs->yclean) != 0) {
            mlab_e2e_obs_free(obs);
            return -1;
        }
    }
    for (i = 0; i < m; ++i) {
        double noise = cfg->sigma * mlab_rng_normal(rng);
        double v = obs->yclean[i] + noise;
        if (v < 0.0) v = 0.0;
        obs->yobs[i] = v;
    }
    obs->n_obs = n;
    obs->sigma = cfg->sigma;
    return 0;
}

/* ---------- 似然 / 后验 ---------- */

double mlab_e2e_nll(const double *theta, int npar, void *ctx)
{
    const mlab_e2e_like_ctx *c = (const mlab_e2e_like_ctx *)ctx;
    double ymodel[2 * MLAB_E2E_MAX_OBS];
    double sse = 0.0, inv_s2;
    int i, m, rc;
    (void)npar;
    if (!c || !c->obs || !theta) return 1e300;
    /* 按 model_id 分派仿真正本（R7）：SEIR 不再静默套用 LV 似然 */
    if (c->model_id == MLAB_E2E_MODEL_SEIR)
        rc = mlab_e2e_seir_simulate(theta, c->y0, c->obs->t, c->obs->n_obs,
                                    c->h_ode, ymodel);
    else
        rc = mlab_e2e_lv_simulate(theta, c->y0, c->obs->t, c->obs->n_obs,
                                  c->h_ode, ymodel);
    if (rc != 0) return 1e200;
    m = 2 * c->obs->n_obs;
    inv_s2 = 1.0 / (c->sigma * c->sigma);
    for (i = 0; i < m; ++i) {
        double r = c->obs->yobs[i] - ymodel[i];
        sse += r * r;
    }
    return 0.5 * sse * inv_s2;
}

double mlab_e2e_logpost(const double *theta, int d, void *ctx)
{
    const mlab_e2e_like_ctx *c = (const mlab_e2e_like_ctx *)ctx;
    int j;
    if (!c || !theta || d <= 0) return -1e300;
    for (j = 0; j < d && j < MLAB_E2E_NPAR; ++j) {
        if (theta[j] < c->lb[j] || theta[j] > c->ub[j]) return -1e300;
    }
    {
        double nll = mlab_e2e_nll(theta, d, ctx);
        if (!(nll < 1e100)) return -1e300;
        return -nll;
    }
}

/* ---------- M2 式混合点估计 ---------- */

int mlab_e2e_point_est(mlab_rng *rng,
                       double (*f)(const double *, int, void *), void *ctx,
                       int dim, const double *lb, const double *ub,
                       int budget,
                       double *theta_hat, double *f_min, int *n_eval)
{
    mlab_de_config cfg;
    mlab_de_result res;
    mlab_objective obj;
    mlab_opt_run run;
    double *lb2 = NULL, *ub2 = NULL, *x = NULL, *xbest = NULL;
    double fbest_all = 1e300;
    int ne = 0, pop, gens, i, rc = -1, restart, n_restart;
    int at_bound;

    if (!rng || !f || dim <= 0 || !lb || !ub || !theta_hat) return -1;
    if (budget < 80) budget = 80;
    /* 多起点：LV 似然存在局部模，单次混合可能失败 */
    n_restart = 3;
    x = (double *)malloc((size_t)dim * sizeof(double));
    xbest = (double *)malloc((size_t)dim * sizeof(double));
    lb2 = (double *)malloc((size_t)dim * sizeof(double));
    ub2 = (double *)malloc((size_t)dim * sizeof(double));
    if (!x || !xbest || !lb2 || !ub2) goto done;
    memset(xbest, 0, (size_t)dim * sizeof(double));  /* R7：未找到有效解时不得返回未初始化内存 */

    for (restart = 0; restart < n_restart; ++restart) {
        double fcur = 1e300;
        at_bound = 0;

        /* 阶段 1：全盒 DE rand/1（M2 全局粗搜） */
        pop = 28;
        if (pop > budget / 2) pop = budget / 2;
        if (pop < 10) pop = 10;
        gens = (budget * 3 / 4) / pop;
        if (gens < 8) gens = 8;
        if (gens > 60) gens = 60;
        memset(&cfg, 0, sizeof cfg);
        cfg.f = f;
        cfg.ctx = ctx;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.pop = pop;
        cfg.max_gen = gens;
        cfg.F = 0.7;
        cfg.CR = 0.85;
        cfg.variant = MLAB_DE_RAND1;
        memset(&res, 0, sizeof res);
        if (mlab_de_run(rng, &cfg, NULL, &res, 0.0) != 1) {
            mlab_de_result_free(&res);
            continue;
        }
        if (res.best_x) memcpy(x, res.best_x, (size_t)dim * sizeof(double));
        else {
            for (i = 0; i < dim; ++i)
                x[i] = lb[i] + (ub[i] - lb[i]) * mlab_rng_uniform(rng);
        }
        fcur = res.best_f;
        ne += res.n_eval;
        mlab_de_result_free(&res);

        /* 阶段 2：缩小盒 DE best/1（完整种群：首行当前最优 + 盒内随机） */
        for (i = 0; i < dim; ++i) {
            double span = ub[i] - lb[i];
            double half = 0.12 * span;
            lb2[i] = x[i] - half;
            ub2[i] = x[i] + half;
            if (lb2[i] < lb[i]) lb2[i] = lb[i];
            if (ub2[i] > ub[i]) ub2[i] = ub[i];
            if (ub2[i] - lb2[i] < 1e-12) {
                lb2[i] = lb[i];
                ub2[i] = ub[i];
            }
        }
        pop = 16;
        gens = budget / 4 / pop;
        if (gens < 4) gens = 4;
        if (gens > 25) gens = 25;
        {
            double *pop0 = (double *)malloc((size_t)pop * (size_t)dim * sizeof(double));
            if (!pop0) {
                free(lb2); free(ub2); free(x); free(xbest);
                return -1;
            }
            memcpy(pop0, x, (size_t)dim * sizeof(double));
            for (i = 1; i < pop; ++i) {
                int k;
                for (k = 0; k < dim; ++k) {
                    double u = mlab_rng_uniform(rng);
                    pop0[i * dim + k] = lb2[k] + (ub2[k] - lb2[k]) * u;
                }
            }
            memset(&cfg, 0, sizeof cfg);
            cfg.f = f;
            cfg.ctx = ctx;
            cfg.dim = dim;
            cfg.lb = lb2;
            cfg.ub = ub2;
            cfg.pop = pop;
            cfg.max_gen = gens;
            cfg.F = 0.45;
            cfg.CR = 0.9;
            cfg.variant = MLAB_DE_BEST1;
            memset(&res, 0, sizeof res);
            if (mlab_de_run(rng, &cfg, pop0, &res, 0.0) == 1) {
                ne += res.n_eval;
                if (res.best_f < fcur && res.best_x) {
                    fcur = res.best_f;
                    memcpy(x, res.best_x, (size_t)dim * sizeof(double));
                }
            }
            mlab_de_result_free(&res);
            free(pop0);
        }

        /* 阶段 3：Nelder-Mead 局部精修 */
        memset(&obj, 0, sizeof obj);
        obj.dim = dim;
        obj.f = f;
        obj.grad = NULL;
        obj.hess = NULL;
        obj.ctx = ctx;
        mlab_opt_run_init(&run, NULL, 0);
        {
            int nm_iters = budget / 4;
            double *x_pre = (double *)malloc((size_t)dim * sizeof(double));
            if (!x_pre) {
                free(lb2); free(ub2); free(x); free(xbest);
                return -1;
            }
            memcpy(x_pre, x, (size_t)dim * sizeof(double));
            if (nm_iters < 80) nm_iters = 80;
            if (nm_iters > 350) nm_iters = 350;
            mlab_opt_neldermead(&obj, x, 1e-12, nm_iters, &run);
            for (i = 0; i < dim; ++i) {
                if (x[i] < lb[i]) x[i] = lb[i];
                if (x[i] > ub[i]) x[i] = ub[i];
            }
            ne += run.fevals + 1;
            {
                /* 盒裁剪后真求值：无改进则回退精修前点，保持 x 与 fcur 一致 */
                double f1 = f(x, dim, ctx);
                if (f1 < fcur) {
                    fcur = f1;
                } else {
                    memcpy(x, x_pre, (size_t)dim * sizeof(double));
                }
            }
            free(x_pre);
        }

        for (i = 0; i < dim; ++i) {
            double span = ub[i] - lb[i];
            if (span <= 0) continue;
            if (x[i] - lb[i] < 1e-4 * span || ub[i] - x[i] < 1e-4 * span)
                at_bound = 1;
        }

        {
            /* 早停条件修正（R7）：对比本次结果与更新前历史最优 fbest_prev。
             * 旧条件在 fbest_all 更新后比较 fcur<=fbest_all*1.05+0.5，
             * 本次即历史最优时恒真 → 只要第 2 次内点就停，多起点鲁棒性名存实亡。 */
            double fbest_prev = fbest_all;
            if (fcur < fbest_all) {
                fbest_all = fcur;
                memcpy(xbest, x, (size_t)dim * sizeof(double));
            }
            if (!at_bound && restart >= 1 && fbest_prev < 1e100 &&
                fcur <= fbest_prev * 1.05 + 0.5)
                break;
        }
    }

    /* 全起点失败：不得用未初始化/全零 xbest 冒充成功（R7） */
    if (!(fbest_all < 1e299)) {
        free(lb2);
        free(ub2);
        free(x);
        free(xbest);
        return -1;
    }

    memcpy(theta_hat, xbest, (size_t)dim * sizeof(double));
    if (f_min) *f_min = f(xbest, dim, ctx);
    if (n_eval) *n_eval = ne;
    rc = 0;
done:
    free(lb2);
    free(ub2);
    free(x);
    free(xbest);
    return rc;
}

/* ---------- MCMC ---------- */

typedef struct {
    const mlab_e2e_like_ctx *like;
    double scale[MLAB_E2E_NPAR];
} m3_scale_ctx;

static double logpost_scaled(const double *x, int d, void *ctx)
{
    const m3_scale_ctx *sc = (const m3_scale_ctx *)ctx;
    double theta[MLAB_E2E_NPAR];
    int j;
    if (!sc || !x || d <= 0) return -1e300;
    for (j = 0; j < d && j < MLAB_E2E_NPAR; ++j)
        theta[j] = x[j] * sc->scale[j];
    return mlab_e2e_logpost(theta, d, (void *)sc->like);
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static double quantile_sorted(const double *sorted, int n, double p)
{
    double idx, frac;
    int i;
    if (n <= 0) return 0.0;
    if (n == 1) return sorted[0];
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    idx = p * (double)(n - 1);
    i = (int)idx;
    frac = idx - (double)i;
    if (i < 0) i = 0;
    if (i + 1 < n) return sorted[i] * (1.0 - frac) + sorted[i + 1] * frac;
    return sorted[n - 1];
}

static void summarize_posterior(mlab_e2e_posterior *post)
{
    int c, j, i, d, ntot;
    double *buf;
    const double **ptrs;
    if (!post || post->n_chains <= 0) return;
    d = post->d;
    ntot = 0;
    for (c = 0; c < post->n_chains; ++c) ntot += post->chains[c].n_keep;
    if (ntot <= 0 || d <= 0) return;
    buf = (double *)malloc((size_t)ntot * sizeof(double));
    if (!buf) return;
    for (j = 0; j < d; ++j) {
        int k = 0;
        double mean = 0.0, var = 0.0;
        for (c = 0; c < post->n_chains; ++c) {
            const mlab_chain *ch = &post->chains[c];
            for (i = 0; i < ch->n_keep; ++i)
                buf[k++] = ch->x[(size_t)i * d + j];
        }
        qsort(buf, (size_t)ntot, sizeof(double), cmp_double);
        mean = mlab_mean(buf, ntot);
        var = mlab_var(buf, ntot);
        post->mean[j] = mean;
        post->sd[j] = (var > 0.0) ? sqrt(var) : 0.0;
        post->cri_lo[j] = quantile_sorted(buf, ntot, 0.025);
        post->cri_hi[j] = quantile_sorted(buf, ntot, 0.975);
    }
    free(buf);

    ptrs = (const double **)malloc((size_t)post->n_chains * sizeof(double *));
    if (ptrs) {
        /* 对齐到最短链长；单链无法计算 R̂，落 0 并由写入侧转 NA（R7） */
        int nmin = post->chains[0].n_keep;
        for (c = 1; c < post->n_chains; ++c)
            if (post->chains[c].n_keep < nmin) nmin = post->chains[c].n_keep;
        if (post->n_chains >= 2 && nmin >= 2) {
            double rhat_per[MLAB_E2E_NPAR];
            for (c = 0; c < post->n_chains; ++c) ptrs[c] = post->chains[c].x;
            post->rhat = mlab_gelman_rubin(ptrs, post->n_chains, nmin, d, rhat_per);
        } else {
            post->rhat = 0.0;
        }
        free(ptrs);
    }
}

int mlab_e2e_mcmc(mlab_rng *rng,
                  const mlab_e2e_like_ctx *like,
                  const double *theta0,
                  int n_burn, int n_keep, int thin, int n_chains,
                  double step0,
                  mlab_e2e_posterior *post)
{
    m3_scale_ctx sc;
    double x0[MLAB_E2E_NPAR];
    double step;
    int c, j, d, pilot_ok;
    mlab_rng pilot_rng;

    if (!rng || !like || !theta0 || !post || n_chains < 1) return -1;
    if (n_chains > MLAB_E2E_NCHAIN_MAX) n_chains = MLAB_E2E_NCHAIN_MAX;
    if (n_burn < 50) n_burn = 50;
    if (n_keep < 100) n_keep = 100;
    if (thin < 1) thin = 1;
    d = MLAB_E2E_NPAR;
    memset(post, 0, sizeof *post);
    post->d = d;
    post->n_chains = n_chains;

    memset(&sc, 0, sizeof sc);
    sc.like = like;
    for (j = 0; j < d; ++j) {
        double s = fabs(theta0[j]);
        double span = like->ub[j] - like->lb[j];
        if (s < 0.25 * span) s = 0.25 * span;
        if (s < 1e-6) s = 1e-6;
        sc.scale[j] = s;
        x0[j] = theta0[j] / sc.scale[j];
    }
    step = (step0 > 0.0) ? step0 : 0.08;

    /* burn-in 内用短链标定缩放空间步长，目标接受率 ~0.25–0.35 */
    pilot_rng = *rng;
    pilot_ok = 0;
    {
        mlab_chain tmp;
        int it;
        memset(&tmp, 0, sizeof tmp);
        for (it = 0; it < 12; ++it) {
            if (!mlab_mh_rwm(&pilot_rng, logpost_scaled, &sc, x0, d, step,
                             100, 200, 1, &tmp))
                break;
            if (tmp.accept_rate > 0.40) step *= 1.20;
            else if (tmp.accept_rate < 0.18) step *= 0.80;
            else {
                pilot_ok = 1;
                mlab_chain_free(&tmp);
                break;
            }
            mlab_chain_free(&tmp);
            if (step < 1e-4) step = 1e-4;
            if (step > 1.0) step = 1.0;
        }
        (void)pilot_ok;
    }
    post->step = step;

    for (c = 0; c < n_chains; ++c) {
        double xc[MLAB_E2E_NPAR];
        unsigned long long seed = mlab_rng_u64(rng);
        mlab_rng cr;
        for (j = 0; j < d; ++j) {
            /* 初始点在盒内分散，便于 R̂ 诊断 */
            double jitter = 0.15 * step * mlab_rng_normal(rng) +
                            0.02 * mlab_rng_normal(rng);
            xc[j] = x0[j] * (1.0 + jitter);
            {
                double lo = like->lb[j] / sc.scale[j];
                double hi = like->ub[j] / sc.scale[j];
                if (xc[j] < lo) xc[j] = 0.5 * (lo + x0[j]);
                if (xc[j] > hi) xc[j] = 0.5 * (hi + x0[j]);
                if (xc[j] < lo) xc[j] = lo + 1e-9;
                if (xc[j] > hi) xc[j] = hi - 1e-9;
            }
        }
        mlab_rng_seed(&cr, seed);
        if (!mlab_mh_rwm(&cr, logpost_scaled, &sc, xc, d, step,
                         n_burn, n_keep, thin, &post->chains[c])) {
            post->n_chains = c;
            mlab_e2e_posterior_free(post);
            return -1;
        }
    }

    /* 链中样本是缩放坐标 z；换算到 θ 再汇总 */
    for (c = 0; c < post->n_chains; ++c) {
        mlab_chain *ch = &post->chains[c];
        int i;
        for (i = 0; i < ch->n_keep; ++i)
            for (j = 0; j < d; ++j)
                ch->x[(size_t)i * d + j] *= sc.scale[j];
    }

    {
        double ar = 0.0;
        for (c = 0; c < post->n_chains; ++c) ar += post->chains[c].accept_rate;
        post->accept_rate = ar / post->n_chains;
    }
    summarize_posterior(post);
    return 0;
}

void mlab_e2e_posterior_free(mlab_e2e_posterior *post)
{
    int c;
    if (!post) return;
    for (c = 0; c < MLAB_E2E_NCHAIN_MAX; ++c)
        mlab_chain_free(&post->chains[c]);
    post->n_chains = 0;
}

/* ---------- 流水线与报告 ---------- */

static int write_obs_csv(const char *path, const mlab_e2e_obs *obs,
                         const double *yclean)
{
    FILE *fp;
    int i;
    if (!path || !obs) return -1;
    fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "t,prey_obs,pred_obs,prey_clean,pred_clean\n");
    for (i = 0; i < obs->n_obs; ++i) {
        fprintf(fp, "%.8g,%.8g,%.8g,%.8g,%.8g\n",
                obs->t[i], obs->yobs[2 * i], obs->yobs[2 * i + 1],
                yclean ? yclean[2 * i] : 0.0,
                yclean ? yclean[2 * i + 1] : 0.0);
    }
    fclose(fp);
    return 0;
}

static int write_theta_csv(const char *path,
                           const mlab_e2e_config *cfg,
                           const double *theta_hat,
                           double f_min,
                           const mlab_e2e_posterior *post)
{
    FILE *fp;
    static const char *names[MLAB_E2E_NPAR] = {"alpha", "beta", "gamma", "delta"};
    int j;
    if (!path || !cfg || !theta_hat) return -1;
    fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "name,true,hat,post_mean,post_sd,cri_lo,cri_hi,hat_err,hat_err_over_sd\n");
    for (j = 0; j < cfg->npar; ++j) {
        double err = theta_hat[j] - cfg->theta_true[j];
        double sd = post ? post->sd[j] : 0.0;
        double ratio = (sd > 1e-15) ? fabs(err) / sd : 1e9;
        fprintf(fp, "%s,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.4f\n",
                names[j], cfg->theta_true[j], theta_hat[j],
                post ? post->mean[j] : 0.0,
                sd,
                post ? post->cri_lo[j] : 0.0,
                post ? post->cri_hi[j] : 0.0,
                err, ratio);
    }
    fprintf(fp, "# nll_hat,%.8g\n", f_min);
    if (post) {
        if (post->n_chains >= 2)
            fprintf(fp, "# rhat,%.6f\n", post->rhat);
        else
            fprintf(fp, "# rhat,NA\n"); /* 单链无 R̂ 定义（R7） */
        fprintf(fp, "# accept_rate,%.4f\n", post->accept_rate);
        fprintf(fp, "# step_scaled,%.6f\n", post->step);
    }
    fclose(fp);
    return 0;
}

static int write_samples_csv(const char *path, const mlab_e2e_posterior *post)
{
    FILE *fp;
    int c, i, j, stride;
    if (!path || !post) return -1;
    fp = fopen(path, "w");
    if (!fp) return -1;
    fprintf(fp, "chain,iter,alpha,beta,gamma,delta\n");
    for (c = 0; c < post->n_chains; ++c) {
        const mlab_chain *ch = &post->chains[c];
        stride = 1;
        if (ch->n_keep > 2000) stride = ch->n_keep / 2000;
        for (i = 0; i < ch->n_keep; i += stride) {
            fprintf(fp, "%d,%d", c, i);
            for (j = 0; j < post->d; ++j)
                fprintf(fp, ",%.8g", ch->x[(size_t)i * post->d + j]);
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return 0;
}

int mlab_e2e_write_report(const char *path,
                          const mlab_e2e_config *cfg,
                          const mlab_e2e_obs *obs,
                          const double *theta_hat,
                          double f_min,
                          int n_eval,
                          const mlab_e2e_posterior *post,
                          int append)
{
    FILE *fp;
    static const char *names[MLAB_E2E_NPAR] = {"alpha", "beta", "gamma", "delta"};
    int j, ok_point = 1, ok_cri = 1;
    if (!path || !cfg || !theta_hat) return -1;
    fp = fopen(path, append ? "a" : "w");
    if (!fp) return -1;
    fprintf(fp, "# M3 端到端参数估计报告\n\n");
    fprintf(fp, "## 配置\n\n");
    fprintf(fp, "- 模型: %s\n", cfg->model_id == MLAB_E2E_MODEL_SEIR ? "SEIR" : "Lotka-Volterra");
    fprintf(fp, "- 真参数 θ: (");
    for (j = 0; j < cfg->npar; ++j)
        fprintf(fp, "%s%.6g", j ? ", " : "", cfg->theta_true[j]);
    fprintf(fp, ")\n");
    fprintf(fp, "- y0 = (%.4g, %.4g), t ∈ [%.3g, %.3g], n_obs=%d, σ=%.4g, h_ode=%.4g\n",
            cfg->y0[0], cfg->y0[1], cfg->t0, cfg->t1, cfg->n_obs, cfg->sigma, cfg->h_ode);
    fprintf(fp, "- 点估计: M2 式混合（DE rand/1 → 缩小盒 DE best/1 → Nelder-Mead），budget 次评估\n");
    fprintf(fp, "- 后验: C2 RWM-MH（缩放坐标），均匀盒先验 + 高斯观测似然\n\n");

    if (obs) {
        double rmse = 0.0;
        int i, m = 2 * obs->n_obs;
        for (i = 0; i < m; ++i) {
            double d = obs->yobs[i] - obs->yclean[i];
            rmse += d * d;
        }
        rmse = (m > 0) ? sqrt(rmse / m) : 0.0;
        fprintf(fp, "## 合成观测\n\n");
        fprintf(fp, "- 样本数: %d 时刻 × 2 物种\n", obs->n_obs);
        fprintf(fp, "- 观测相对真轨迹 RMSE: %.4g（噪声水平 σ=%.4g）\n\n", rmse, obs->sigma);
    }

    fprintf(fp, "## 点估计与后验\n\n");
    fprintf(fp, "| 参数 | 真值 | 点估计 | 后验均值 | 后验 sd | 95%% CrI | |err|/sd |\n");
    fprintf(fp, "|---|---|---|---|---|---|---|\n");
    for (j = 0; j < cfg->npar; ++j) {
        double err = fabs(theta_hat[j] - cfg->theta_true[j]);
        double sd = post ? post->sd[j] : 0.0;
        double ratio = (sd > 1e-15) ? err / sd : 1e9;
        fprintf(fp, "| %s | %.6g | %.6g | %.6g | %.6g | [%.6g, %.6g] | %.3f |\n",
                names[j], cfg->theta_true[j], theta_hat[j],
                post ? post->mean[j] : 0.0, sd,
                post ? post->cri_lo[j] : 0.0,
                post ? post->cri_hi[j] : 0.0,
                ratio);
        if (post) {
            if (cfg->theta_true[j] < post->cri_lo[j] || cfg->theta_true[j] > post->cri_hi[j])
                ok_cri = 0;
            if (ratio >= 2.0) ok_point = 0;
        }
    }
    fprintf(fp, "\n");
    fprintf(fp, "- NLL(θ̂) = %.6g, 评估数 ≈ %d\n", f_min, n_eval);
    if (post) {
        if (post->n_chains >= 2)
            fprintf(fp, "- 后验 R̂ = %.4f, 平均接受率 = %.3f, 缩放步长 = %.4f\n",
                    post->rhat, post->accept_rate, post->step);
        else
            fprintf(fp, "- 后验 R̂ = NA（单链，不适用）, 平均接受率 = %.3f, 缩放步长 = %.4f\n",
                    post->accept_rate, post->step);
    }
    fprintf(fp, "- 判据点估计 |err|<2sd: %s\n", ok_point ? "PASS" : "FAIL");
    fprintf(fp, "- 判据 CrI 含真值（本数据集）: %s\n", ok_cri ? "PASS" : "PARTIAL");
    fprintf(fp, "\n## 中间产物\n\n");
    fprintf(fp, "- `results/m3_obs.csv` 观测与真轨迹\n");
    fprintf(fp, "- `results/m3_theta.csv` 点估计/后验/CrI\n");
    fprintf(fp, "- `results/m3_mcmc_samples.csv` 后验样本（降采样）\n");
    fprintf(fp, "- 覆盖率实验见 `results/m3_coverage.csv`\n");
    fclose(fp);
    return 0;
}

int mlab_e2e_pipeline(mlab_rng *rng,
                      const mlab_e2e_config *cfg,
                      unsigned data_seed,
                      unsigned mcmc_seed,
                      int budget,
                      int n_burn, int n_keep, int thin, int n_chains,
                      const char *results_dir,
                      double *theta_hat,
                      double *f_min,
                      int *n_eval,
                      mlab_e2e_obs *obs_out,
                      mlab_e2e_posterior *post_out)
{
    mlab_e2e_like_ctx like;
    mlab_e2e_obs obs;
    mlab_e2e_posterior post;
    mlab_rng data_rng, mcmc_rng;
    double th_hat[MLAB_E2E_NPAR];
    double fbest = 0.0;
    int ne = 0, rc;
    char path[256];

    if (!rng || !cfg || !theta_hat) return -1;
    memset(&obs, 0, sizeof obs);
    memset(&post, 0, sizeof post);
    memset(th_hat, 0, sizeof th_hat);

    mlab_rng_seed(&data_rng, data_seed);
    if (mlab_e2e_synth(&data_rng, cfg, &obs) != 0) return -1;

    memset(&like, 0, sizeof like);
    like.obs = &obs;
    like.y0 = cfg->y0;
    like.sigma = cfg->sigma;
    like.h_ode = cfg->h_ode;
    memcpy(like.lb, cfg->lb, sizeof like.lb);
    memcpy(like.ub, cfg->ub, sizeof like.ub);
    like.model_id = cfg->model_id;

    /* 点估计：盒内随机起点由 DE 覆盖 */
    rc = mlab_e2e_point_est(rng, mlab_e2e_nll, &like,
                            cfg->npar, cfg->lb, cfg->ub,
                            budget, th_hat, &fbest, &ne);
    if (rc != 0) {
        mlab_e2e_obs_free(&obs);
        return -1;
    }

    mlab_rng_seed(&mcmc_rng, mcmc_seed);
    rc = mlab_e2e_mcmc(&mcmc_rng, &like, th_hat,
                       n_burn, n_keep, thin, n_chains, 0.0, &post);
    if (rc != 0) {
        mlab_e2e_obs_free(&obs);
        return -1;
    }

    memcpy(theta_hat, th_hat, sizeof th_hat);
    if (f_min) *f_min = fbest;
    if (n_eval) *n_eval = ne;

    if (results_dir && results_dir[0]) {
        snprintf(path, sizeof path, "%s/m3_obs.csv", results_dir);
        write_obs_csv(path, &obs, obs.yclean);
        snprintf(path, sizeof path, "%s/m3_theta.csv", results_dir);
        write_theta_csv(path, cfg, th_hat, fbest, &post);
        snprintf(path, sizeof path, "%s/m3_mcmc_samples.csv", results_dir);
        write_samples_csv(path, &post);
        snprintf(path, sizeof path, "%s/m3_report.md", results_dir);
        mlab_e2e_write_report(path, cfg, &obs, th_hat, fbest, ne, &post, 0);
    }

    if (obs_out) {
        *obs_out = obs;
        memset(&obs, 0, sizeof obs); /* 所有权移交 */
    } else {
        mlab_e2e_obs_free(&obs);
    }
    if (post_out) {
        *post_out = post;
        memset(&post, 0, sizeof post);
    } else {
        mlab_e2e_posterior_free(&post);
    }
    return 0;
}
