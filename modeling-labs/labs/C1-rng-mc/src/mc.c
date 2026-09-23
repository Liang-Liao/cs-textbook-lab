#include "mc.h"
#include "stats.h"

#include <math.h>
#include <stdlib.h>

double mlab_mc_box(mlab_rng *r, mlab_mc_box_fn f, void *ctx, int d,
                   const double *lb, const double *ub, long n,
                   mlab_mc_result *out)
{
    double *x = NULL;
    double *vals = NULL;
    double vol = 1.0, mean, var;
    long i;
    int j;

    if (!r || !f || d <= 0 || n <= 1 || !lb || !ub) {
        if (out) {
            out->mean = out->var = out->se = 0.0;
            out->n = 0;
        }
        return 0.0;
    }
    for (j = 0; j < d; ++j) vol *= (ub[j] - lb[j]);
    x = (double *)malloc((size_t)d * sizeof(double));
    vals = (double *)malloc((size_t)n * sizeof(double));
    if (!x || !vals) {
        free(x);
        free(vals);
        return 0.0;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < d; ++j)
            x[j] = lb[j] + (ub[j] - lb[j]) * mlab_rng_uniform(r);
        vals[i] = f(x, d, ctx);
    }
    mean = mlab_mean(vals, (int)n);
    var = mlab_var(vals, (int)n);
    free(x);
    free(vals);
    if (out) {
        out->mean = mean;
        out->var = var;
        out->se = sqrt(var / (double)n);
        out->n = n;
    }
    return vol * mean;
}

double mlab_mc_hit_miss(mlab_rng *r, mlab_mc_ind_fn inside, void *ctx, int d,
                        const double *lb, const double *ub, long n,
                        mlab_mc_result *out)
{
    double *x;
    double *vals;
    long i, hits = 0;
    double mean, var;
    int j;

    if (!r || !inside || d <= 0 || n <= 1 || !lb || !ub) {
        if (out) {
            out->mean = out->var = out->se = 0.0;
            out->n = 0;
        }
        return 0.0;
    }
    x = (double *)malloc((size_t)d * sizeof(double));
    vals = (double *)malloc((size_t)n * sizeof(double));
    if (!x || !vals) {
        free(x);
        free(vals);
        return 0.0;
    }
    for (i = 0; i < n; ++i) {
        int ok;
        for (j = 0; j < d; ++j)
            x[j] = lb[j] + (ub[j] - lb[j]) * mlab_rng_uniform(r);
        ok = inside(x, d, ctx) ? 1 : 0;
        vals[i] = (double)ok;
        hits += ok;
    }
    mean = mlab_mean(vals, (int)n);
    var = mlab_var(vals, (int)n);
    free(x);
    free(vals);
    if (out) {
        out->mean = mean;
        out->var = var;
        out->se = sqrt(var / (double)n);
        out->n = n;
    }
    (void)hits;
    return mean;
}

double mlab_mc_is(mlab_rng *r,
                  mlab_scalar_fn f, void *fctx,
                  mlab_scalar_fn q_density, void *qctx,
                  mlab_sample_fn q_sample, void *sctx,
                  long n, mlab_mc_result *out)
{
    double *vals;
    long i;
    double mean, var;

    if (!r || !f || !q_density || !q_sample || n <= 1) {
        if (out) {
            out->mean = out->var = out->se = 0.0;
            out->n = 0;
        }
        return 0.0;
    }
    vals = (double *)malloc((size_t)n * sizeof(double));
    if (!vals) return 0.0;
    for (i = 0; i < n; ++i) {
        double x = q_sample(r, sctx);
        double q = q_density(x, qctx);
        vals[i] = (q > 0.0) ? (f(x, fctx) / q) : 0.0;
    }
    mean = mlab_mean(vals, (int)n);
    var = mlab_var(vals, (int)n);
    free(vals);
    if (out) {
        out->mean = mean;
        out->var = var;
        out->se = sqrt(var / (double)n);
        out->n = n;
    }
    return mean;
}

double mlab_mc_is_weighted(mlab_rng *r,
                           mlab_scalar_fn p_density, void *pctx,
                           mlab_scalar_fn f, void *fctx,
                           mlab_scalar_fn q_density, void *qctx,
                           mlab_sample_fn q_sample, void *sctx,
                           long n, mlab_mc_result *out,
                           mlab_mc_is_diag *diag)
{
    double *vals, *w;
    long i;
    double mean, var_f, se, sw = 0.0, sw2 = 0.0, wmax = 0.0, ess;

    if (!r || !f || !q_density || !q_sample || !p_density || n <= 1) {
        if (out) {
            out->mean = out->var = out->se = 0.0;
            out->n = 0;
        }
        if (diag) {
            diag->ess = diag->ess_ratio = diag->w_cv2 = diag->w_max_ratio = 0.0;
        }
        return 0.0;
    }
    vals = (double *)malloc((size_t)n * sizeof(double));
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!vals || !w) {
        free(vals);
        free(w);
        return 0.0;
    }
    for (i = 0; i < n; ++i) {
        double x = q_sample(r, sctx);
        double q = q_density(x, qctx);
        if (q > 0.0) {
            w[i] = p_density(x, pctx) / q;
            vals[i] = f(x, fctx);
        } else {
            w[i] = 0.0;
            vals[i] = 0.0;
        }
        sw += w[i];
        sw2 += w[i] * w[i];
        if (w[i] > wmax) wmax = w[i];
    }
    /* 自归一化 IS：E_p[f] ≈ Σ w f / Σ w */
    {
        double num = 0.0;
        for (i = 0; i < n; ++i) num += w[i] * vals[i];
        mean = sw > 0.0 ? num / sw : 0.0;
    }
    ess = sw2 > 0.0 ? sw * sw / sw2 : 0.0;
    /* 估计量方差 ≈ f 的加权样本方差 / ESS */
    var_f = 0.0;
    for (i = 0; i < n; ++i) {
        double d = vals[i] - mean;
        var_f += w[i] * d * d;
    }
    var_f = sw > 0.0 && ess > 1.0 ? var_f / sw : 0.0;
    se = var_f > 0.0 && ess > 1.0 ? sqrt(var_f / ess) : 0.0;
    free(vals);
    free(w);
    if (out) {
        out->mean = mean;
        out->var = var_f;
        out->se = se;
        out->n = n;
    }
    if (diag) {
        diag->ess = ess;
        diag->ess_ratio = ess / (double)n;
        diag->w_cv2 = sw > 0.0 ? (double)n * sw2 / (sw * sw) - 1.0 : 0.0;
        diag->w_max_ratio = sw > 0.0 ? wmax / sw : 0.0;
    }
    return mean;
}

int mlab_rejection_sample(mlab_rng *r,
                          mlab_scalar_fn target, void *tctx,
                          mlab_sample_fn env_sample, void *sctx,
                          mlab_scalar_fn env_pdf, void *ectx,
                          double c, long max_trials,
                          double *out, long *n_trials)
{
    long t = 0;
    if (!r || !target || !env_sample || !env_pdf || c <= 0.0 || max_trials <= 0)
        return 0;
    while (t < max_trials) {
        double y = env_sample(r, sctx);
        double g = env_pdf(y, ectx);
        double fy = target(y, tctx);
        double acc_bound = c * g;
        ++t;
        if (acc_bound <= 0.0) continue;
        if (mlab_rng_uniform(r) * acc_bound <= fy) {
            if (out) *out = y;
            if (n_trials) *n_trials += t;
            return 1;
        }
    }
    if (n_trials) *n_trials += t;
    return 0;
}
