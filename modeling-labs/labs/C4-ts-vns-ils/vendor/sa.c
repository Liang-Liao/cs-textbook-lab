#include "sa.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_sa_result_free(mlab_sa_result *r)
{
    if (!r) return;
    free(r->x_best);
    r->x_best = NULL;
}

int mlab_sa_metropolis(double df, double T, mlab_rng *rng)
{
    double u;
    if (df <= 0.0) return 1;
    if (T <= 0.0) return 0;
    u = mlab_rng_uniform(rng);
    return log(u) < -df / T;
}

static void clip_box(const mlab_sa_problem *p, double *x)
{
    int i;
    if (!p->lb || !p->ub) return;
    for (i = 0; i < p->dim; ++i) {
        if (x[i] < p->lb[i]) x[i] = p->lb[i];
        if (x[i] > p->ub[i]) x[i] = p->ub[i];
    }
}

static double range_scale(const mlab_sa_problem *p, int j)
{
    if (p->lb && p->ub) {
        double r = p->ub[j] - p->lb[j];
        return r > 0 ? r : 1.0;
    }
    return 1.0;
}

double mlab_sa_calibrate_T0(mlab_rng *rng,
                            const mlab_sa_problem *prob,
                            const double *x0,
                            double step_scale,
                            double target_accept,
                            int n_probe)
{
    double *y;
    double f0, sum_df = 0.0;
    int n_up = 0, i, j, d;
    double mean_df, denom;

    if (!rng || !prob || !prob->f || !x0 || n_probe < 8) return 1.0;
    if (target_accept <= 0.0 || target_accept >= 1.0) target_accept = 0.8;
    d = prob->dim;
    y = (double *)malloc((size_t)d * sizeof(double));
    if (!y) return 1.0;
    f0 = prob->f(x0, d, prob->ctx);
    for (i = 0; i < n_probe; ++i) {
        double fy, df;
        for (j = 0; j < d; ++j) {
            double sig = step_scale * range_scale(prob, j);
            y[j] = x0[j] + sig * mlab_rng_normal(rng);
        }
        clip_box(prob, y);
        fy = prob->f(y, d, prob->ctx);
        df = fy - f0;
        if (df > 0) {
            sum_df += df;
            ++n_up;
        }
    }
    free(y);
    mean_df = n_up > 0 ? sum_df / n_up : 0.0;
    if (mean_df <= 1e-12) {
        /* 平坦：给一个中等温度 */
        return 1.0;
    }
    denom = -log(target_accept);
    if (denom <= 1e-12) denom = 0.223;
    return mean_df / denom;
}

static int sa_run(mlab_rng *rng,
                  const mlab_sa_problem *prob,
                  const mlab_sa_config *cfg,
                  const double *x0,
                  mlab_sa_result *out,
                  double *diag_T, double *diag_acc, int diag_cap)
{
    int d, j, outer, max_outer, n_diag = 0;
    double T, T0, f_cur, f_best;
    double *x, *y, *x_best;
    long n_prop = 0, n_acc = 0;
    mlab_sa_config c;

    if (!rng || !prob || !prob->f || !cfg || !x0 || !out) return 0;
    d = prob->dim;
    if (d <= 0 || cfg->alpha <= 0.0 || cfg->alpha >= 1.0 || cfg->inner <= 0)
        return 0;
    c = *cfg;
    if (c.target_accept <= 0 || c.target_accept >= 1) c.target_accept = 0.8;
    if (c.step_scale <= 0) c.step_scale = 0.1;
    if (c.T_min <= 0) c.T_min = 1e-4;
    if (c.schedule != MLAB_SA_SCHEDULE_LOG) c.schedule = MLAB_SA_SCHEDULE_GEOMETRIC;
    if (c.reheat_every < 0) c.reheat_every = 0;
    if (c.reheat_frac <= 0.0 || c.reheat_frac > 1.0) c.reheat_frac = 0.5;
    T0 = c.T0;
    if (T0 <= 0) T0 = 1.0;
    if (c.calibrate_T0) {
        T0 = mlab_sa_calibrate_T0(rng, prob, x0, c.step_scale,
                                  c.target_accept, 40);
        if (!(T0 > 0)) T0 = 1.0;
    }
    max_outer = c.max_outer;
    if (max_outer <= 0) {
        if (c.schedule == MLAB_SA_SCHEDULE_LOG) {
            max_outer = 1000; /* 对数降温无 alpha 可推，给保守默认 */
        } else {
            max_outer = (int)(log(c.T_min / T0) / log(c.alpha)) + 2;
            if (max_outer < 10) max_outer = 10;
            if (max_outer > 20000) max_outer = 20000;
        }
    }

    x = (double *)malloc((size_t)d * sizeof(double));
    y = (double *)malloc((size_t)d * sizeof(double));
    x_best = (double *)malloc((size_t)d * sizeof(double));
    if (!x || !y || !x_best) {
        free(x);
        free(y);
        free(x_best);
        return 0;
    }
    memcpy(x, x0, (size_t)d * sizeof(double));
    memcpy(x_best, x0, (size_t)d * sizeof(double));
    f_cur = prob->f(x, d, prob->ctx);
    f_best = f_cur;
    memset(out, 0, sizeof *out);
    out->x_best = x_best;
    out->T0_used = T0;

    T = T0;
    for (outer = 0; outer < max_outer && T > c.T_min; ++outer) {
        int acc_layer = 0;
        int k;
        for (k = 0; k < c.inner; ++k) {
            double fy, sig_ratio = c.step_fixed ? 1.0 : (T / T0);
            for (j = 0; j < d; ++j) {
                double sig = c.step_scale * sig_ratio * range_scale(prob, j);
                y[j] = x[j] + sig * mlab_rng_normal(rng);
            }
            clip_box(prob, y);
            /* 盒外拒绝视为不接受（已 clip，仍计算） */
            fy = prob->f(y, d, prob->ctx);
            ++n_prop;
            ++out->n_fevals;
            if (mlab_sa_metropolis(fy - f_cur, T, rng)) {
                memcpy(x, y, (size_t)d * sizeof(double));
                f_cur = fy;
                ++n_acc;
                ++acc_layer;
                if (f_cur < f_best) {
                    f_best = f_cur;
                    memcpy(x_best, x, (size_t)d * sizeof(double));
                }
            }
        }
        if (diag_T && diag_acc && n_diag < diag_cap) {
            diag_T[n_diag] = T;
            diag_acc[n_diag] = (double)acc_layer / (double)c.inner;
            ++n_diag;
        }
        /* 降温（几何乘性 / 对数慢降温）+ 可选周期重加热 */
        if (c.schedule == MLAB_SA_SCHEDULE_LOG) {
            T = T0 * log(2.0) / log(1.0 + (double)(outer + 2));
        } else {
            T *= c.alpha;
        }
        if (c.reheat_every > 0 && (outer + 1) % c.reheat_every == 0)
            T = c.reheat_frac * T0;
        ++out->n_outer;
    }
    out->f_best = f_best;
    out->f_final = f_cur;
    out->n_accept = (int)n_acc;
    out->accept_rate = n_prop > 0 ? (double)n_acc / (double)n_prop : 0.0;
    free(x);
    free(y);
    return 1;
}

int mlab_sa_continuous(mlab_rng *rng,
                       const mlab_sa_problem *prob,
                       const mlab_sa_config *cfg,
                       const double *x0,
                       mlab_sa_result *out)
{
    return sa_run(rng, prob, cfg, x0, out, NULL, NULL, 0);
}

int mlab_sa_continuous_diag(mlab_rng *rng,
                            const mlab_sa_problem *prob,
                            const mlab_sa_config *cfg,
                            const double *x0,
                            mlab_sa_result *out,
                            double *diag_T, double *diag_acc, int diag_cap)
{
    return sa_run(rng, prob, cfg, x0, out, diag_T, diag_acc, diag_cap);
}

int mlab_sa_multirestart(mlab_rng *rng,
                         const mlab_sa_problem *prob,
                         const mlab_sa_config *cfg,
                         int n_restarts,
                         mlab_sa_result *out)
{
    int d, k, have_box;
    double *lb, *ub;
    mlab_sa_result best, r;
    double f_best;
    int ok = 0;

    if (!rng || !prob || !prob->f || !cfg || !out || n_restarts < 1) return 0;
    d = prob->dim;
    if (d <= 0) return 0;
    have_box = (prob->lb && prob->ub);
    lb = (double *)malloc((size_t)d * sizeof(double));
    ub = (double *)malloc((size_t)d * sizeof(double));
    if (!lb || !ub) {
        free(lb);
        free(ub);
        return 0;
    }
    if (have_box) {
        memcpy(lb, prob->lb, (size_t)d * sizeof(double));
        memcpy(ub, prob->ub, (size_t)d * sizeof(double));
    }
    memset(&best, 0, sizeof best);
    f_best = 1e300;
    for (k = 0; k < n_restarts; ++k) {
        double *x0 = (double *)malloc((size_t)d * sizeof(double));
        int j;
        if (!x0) break;
        for (j = 0; j < d; ++j) {
            if (have_box)
                x0[j] = lb[j] + mlab_rng_uniform(rng) * (ub[j] - lb[j]);
            else
                x0[j] = mlab_rng_normal(rng);
        }
        memset(&r, 0, sizeof r);
        if (mlab_sa_continuous(rng, prob, cfg, x0, &r)) {
            ok = 1;
            if (r.f_best < f_best) {
                mlab_sa_result_free(&best);
                best = r;
                f_best = r.f_best;
                r.x_best = NULL; /* 所有权移入 best */
            } else {
                mlab_sa_result_free(&r);
            }
        } else {
            mlab_sa_result_free(&r);
        }
        free(x0);
    }
    free(lb);
    free(ub);
    if (!ok) return 0;
    *out = best;
    return 1;
}
