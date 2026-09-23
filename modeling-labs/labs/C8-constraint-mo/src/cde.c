#include "cde.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_cde_result_free(mlab_cde_result *r)
{
    if (!r) return;
    free(r->best_x);
    r->best_x = NULL;
}

double mlab_cde_violation(const double *x, int n, mlab_cde_ineq ineq,
                          int m, void *ctx)
{
    double *g;
    double v = 0.0;
    int i;
    if (!ineq || m <= 0) return 0.0;
    g = (double *)malloc((size_t)m * sizeof(double));
    if (!g) return 0.0;
    memset(g, 0, (size_t)m * sizeof(double));
    ineq(x, n, g, m, ctx);
    for (i = 0; i < m; ++i)
        if (g[i] > 0.0) v += g[i];
    free(g);
    return v;
}

int mlab_deb_better(double fa, double va, double fb, double vb)
{
    int fea = va <= 1e-12, feb = vb <= 1e-12;
    if (fea && feb) return fa < fb;
    if (fea && !feb) return 1;
    if (!fea && feb) return 0;
    return va < vb;
}

double mlab_prob_disk_f(const double *x, int n, void *ctx)
{
    double s = 0.0;
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) {
        double d = x[i] - 1.5;
        s += d * d;
    }
    return s;
}

void mlab_prob_disk_g(const double *x, int n, double *g, int m, void *ctx)
{
    double s = 0.0, r2 = 1.0;
    int i;
    if (ctx) r2 = *(const double *)ctx;
    r2 = r2 * r2;
    if (m < 1 || !g) return;
    for (i = 0; i < n; ++i) s += x[i] * x[i];
    g[0] = s - r2; /* <=0 */
    for (i = 1; i < m; ++i) g[i] = 0.0;
}

double mlab_prob_disk_fstar(int n, double r)
{
    /* x* = r/sqrt(n) * ones，f* = n*(r/sqrt(n) - 1.5)^2 */
    double t = r / sqrt((double)n);
    double d = t - 1.5;
    return (double)n * d * d;
}

double mlab_prob_disk_box(double r)
{
    /* 盒半宽：让可行体积占比足够小，死亡惩罚难命中 */
    return 3.0 + 2.0 * (1.0 - r);
}

static void clip_box(double *x, int dim, const double *lb, const double *ub)
{
    int d;
    if (!lb || !ub) return;
    for (d = 0; d < dim; ++d) {
        if (x[d] < lb[d]) x[d] = lb[d];
        if (x[d] > ub[d]) x[d] = ub[d];
    }
}

/* 球约束修复（投影）：||x||>r 时缩放回球面；ctx 为 double* 半径，空则 r=1 */
void mlab_prob_disk_repair(double *x, int n, void *ctx)
{
    double r = 1.0, s = 0.0, scale;
    int i;
    if (ctx) r = *(const double *)ctx;
    for (i = 0; i < n; ++i) s += x[i] * x[i];
    if (s <= r * r) return;
    scale = r / sqrt(s);
    for (i = 0; i < n; ++i) x[i] *= scale;
}

int mlab_cde_run(mlab_rng *rng, const mlab_cde_config *cfg, mlab_cde_result *out)
{
    int dim, np, g, i, d;
    double *pop, *trial, *fit, *viol, *score;
    double F, CR, pen, mu;
    int max_gen, m;
    double viol_min_run = 1e300;

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0 || cfg->pop < 4)
        return 0;
    dim = cfg->dim;
    np = cfg->pop;
    F = cfg->F > 0 ? cfg->F : 0.5;
    CR = cfg->CR > 0 ? cfg->CR : 0.9;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 200;
    m = cfg->m_ineq;
    pen = cfg->death_penalty > 0 ? cfg->death_penalty : 1e20;
    mu = cfg->penalty_mu > 0 ? cfg->penalty_mu : 1.0;

    memset(out, 0, sizeof *out);
    pop = (double *)malloc((size_t)np * dim * sizeof(double));
    trial = (double *)malloc((size_t)dim * sizeof(double));
    fit = (double *)malloc((size_t)np * sizeof(double));
    viol = (double *)malloc((size_t)np * sizeof(double));
    score = (double *)malloc((size_t)np * sizeof(double));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    if (!pop || !trial || !fit || !viol || !score || !out->best_x) {
        free(pop); free(trial); free(fit); free(viol); free(score);
        mlab_cde_result_free(out);
        return 0;
    }

    if (cfg->lb && cfg->ub) {
        for (i = 0; i < np; ++i)
            for (d = 0; d < dim; ++d)
                pop[i * dim + d] = cfg->lb[d] +
                    (cfg->ub[d] - cfg->lb[d]) * mlab_rng_uniform(rng);
    } else {
        for (i = 0; i < np * dim; ++i)
            pop[i] = mlab_rng_normal(rng);
    }

    out->best_f = 1e300;
    out->best_viol = 1e300;
    out->feasible = 0;
    out->viol_final = 1e300;
    out->mu_final = mu;

    for (g = 0; g <= max_gen; ++g) {
        int n_feas = 0;

        /* 父代只在初始代整群评估一次；之后 fit/viol 由选择步维护，
         * 每代只评 np 个 trial（评估口径与 C5 de 对齐） */
        if (g == 0) {
            for (i = 0; i < np; ++i) {
                fit[i] = cfg->f(&pop[i * dim], dim, cfg->ctx);
                viol[i] = mlab_cde_violation(&pop[i * dim], dim, cfg->ineq,
                                             m, cfg->ineq_ctx);
                ++out->n_eval;
            }
        }
        for (i = 0; i < np; ++i) {
            if (viol[i] < viol_min_run) viol_min_run = viol[i];
            if (viol[i] <= 1e-12) {
                ++n_feas;
                if (fit[i] < out->best_f) {
                    out->best_f = fit[i];
                    out->best_viol = 0.0;
                    out->feasible = 1;
                    memcpy(out->best_x, &pop[i * dim],
                           (size_t)dim * sizeof(double));
                }
            }
            switch (cfg->mode) {
            case MLAB_CDE_DEATH:
                score[i] = (viol[i] <= 1e-12) ? fit[i] : pen;
                break;
            case MLAB_CDE_STATIC_PEN:
                score[i] = fit[i] + mu * viol[i];
                break;
            case MLAB_CDE_ADAPT_PEN:
                score[i] = fit[i] + mu * viol[i];
                break;
            case MLAB_CDE_DEB:
            default:
                score[i] = fit[i]; /* Deb 模式选择不用标量分 */
                break;
            }
        }

        /* 自适应罚：上一代可行比例驱动 μ（Hadj-Alouane 风格乘性调整） */
        if (cfg->mode == MLAB_CDE_ADAPT_PEN && g > 0) {
            double beta = cfg->adapt_mu_factor > 1.0 ? cfg->adapt_mu_factor : 2.0;
            if (n_feas == 0)
                mu *= beta;          /* 全不可行：加大罚压 */
            else if (n_feas == np)
                mu /= beta;          /* 全可行：放松，避免过罚 */
            if (mu < 1e-12) mu = 1e-12;
            if (mu > 1e18) mu = 1e18;
            out->mu_final = mu;
        }

        /* 末代统计：末代最优违反度 = 种群中最小违反度 */
        {
            double bmin = 1e300;
            for (i = 0; i < np; ++i)
                if (viol[i] < bmin) bmin = viol[i];
            out->viol_final = bmin;
        }
        out->n_feas_final = n_feas;
        if (g == max_gen) break;

        for (i = 0; i < np; ++i) {
            int r1, r2, r3, jrand;
            do { r1 = (int)(mlab_rng_uniform(rng) * np) % np; } while (r1 == i);
            do { r2 = (int)(mlab_rng_uniform(rng) * np) % np; } while (r2 == i || r2 == r1);
            do { r3 = (int)(mlab_rng_uniform(rng) * np) % np; } while (r3 == i || r3 == r1 || r3 == r2);
            jrand = (int)(mlab_rng_uniform(rng) * dim) % dim;
            for (d = 0; d < dim; ++d) {
                trial[d] = pop[r1 * dim + d] +
                           F * (pop[r2 * dim + d] - pop[r3 * dim + d]);
                if (!(mlab_rng_uniform(rng) < CR) && d != jrand)
                    trial[d] = pop[i * dim + d];
            }
            clip_box(trial, dim, cfg->lb, cfg->ub);
            if (cfg->repair)
                cfg->repair(trial, dim, cfg->repair_ctx);
            {
                double ft = cfg->f(trial, dim, cfg->ctx);
                double vt = mlab_cde_violation(trial, dim, cfg->ineq,
                                               m, cfg->ineq_ctx);
                double st;
                int take;
                ++out->n_eval;
                if (vt < viol_min_run) viol_min_run = vt;
                switch (cfg->mode) {
                case MLAB_CDE_DEATH:
                    st = (vt <= 1e-12) ? ft : pen;
                    /* 不可行 vs 不可行：中性（无违反度梯度），可行分直接比 */
                    take = (vt > 1e-12 && viol[i] > 1e-12) ? 1 : (st <= score[i]);
                    break;
                case MLAB_CDE_STATIC_PEN:
                case MLAB_CDE_ADAPT_PEN:
                    st = ft + mu * vt;
                    take = st <= score[i];
                    break;
                case MLAB_CDE_DEB:
                default:
                    st = ft;
                    take = mlab_deb_better(ft, vt, fit[i], viol[i]);
                    break;
                }
                if (take) {
                    memcpy(&pop[i * dim], trial, (size_t)dim * sizeof(double));
                    fit[i] = ft;
                    viol[i] = vt;
                    score[i] = st;
                    if (vt <= 1e-12 && ft < out->best_f) {
                        out->best_f = ft;
                        out->best_viol = 0.0;
                        out->feasible = 1;
                        memcpy(out->best_x, trial, (size_t)dim * sizeof(double));
                    }
                }
            }
        }
        out->gen_used = g + 1;
    }

    out->viol_min = viol_min_run;
    if (!out->feasible) {
        /* 无可行解：报告末代 Deb 意义下代表解（用缓存数组，不再评估） */
        double bf = 1e300, bv = 1e300;
        int bi = -1;
        for (i = 0; i < np; ++i) {
            if (bi < 0 || mlab_deb_better(fit[i], viol[i], bf, bv)) {
                bf = fit[i];
                bv = viol[i];
                bi = i;
            }
        }
        if (bi >= 0) {
            out->best_f = bf;
            out->best_viol = bv;
            memcpy(out->best_x, &pop[bi * dim], (size_t)dim * sizeof(double));
        }
    }

    free(pop); free(trial); free(fit); free(viol); free(score);
    return 1;
}
