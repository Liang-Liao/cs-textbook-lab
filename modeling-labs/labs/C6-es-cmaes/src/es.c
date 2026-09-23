#include "es.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_es1p1_result_free(mlab_es1p1_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->sigma_hist);
    free(r->success_hist);
    r->best_x = NULL;
    r->sigma_hist = NULL;
    r->success_hist = NULL;
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

int mlab_es1p1_run(mlab_rng *rng, const mlab_es1p1_config *cfg,
                   const double *x0, mlab_es1p1_result *out)
{
    int dim, g, d, max_gen, window;
    double *x, *trial, *win;
    double sigma, p_target, factor, fcur;
    int win_pos = 0, win_fill = 0, n_succ_win = 0;

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0)
        return 0;
    dim = cfg->dim;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 200;
    p_target = cfg->p_target > 0.0 ? cfg->p_target : 0.2;
    window = cfg->window > 0 ? cfg->window : 20;
    factor = cfg->adapt_factor > 0.0 ? cfg->adapt_factor : 1.22;
    sigma = cfg->sigma0 > 0.0 ? cfg->sigma0 : 1.0;

    memset(out, 0, sizeof *out);
    x = (double *)malloc((size_t)dim * sizeof(double));
    trial = (double *)malloc((size_t)dim * sizeof(double));
    win = (double *)calloc((size_t)window, sizeof(double));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->sigma_hist = (double *)malloc((size_t)(max_gen + 2) * sizeof(double));
    out->success_hist = (double *)malloc((size_t)(max_gen + 2) * sizeof(double));
    out->n_eval = 0;
    out->hist_len = 0;
    if (!x || !trial || !win || !out->best_x || !out->sigma_hist || !out->success_hist) {
        free(x); free(trial); free(win);
        mlab_es1p1_result_free(out);
        return 0;
    }

    if (x0) {
        memcpy(x, x0, (size_t)dim * sizeof(double));
    } else {
        double s = cfg->x0_scale > 0.0 ? cfg->x0_scale : 2.0;
        for (d = 0; d < dim; ++d)
            x[d] = (2.0 * mlab_rng_uniform(rng) - 1.0) * s;
    }
    clip_box(x, dim, cfg->lb, cfg->ub);
    fcur = cfg->f(x, dim, cfg->ctx);
    ++out->n_eval;
    out->best_f = fcur;
    memcpy(out->best_x, x, (size_t)dim * sizeof(double));

    for (g = 0; g < max_gen; ++g) {
        int success;
        double fnew, p_hat;
        out->sigma_hist[out->hist_len] = sigma;
        for (d = 0; d < dim; ++d)
            trial[d] = x[d] + sigma * mlab_rng_normal(rng);
        clip_box(trial, dim, cfg->lb, cfg->ub);
        fnew = cfg->f(trial, dim, cfg->ctx);
        ++out->n_eval;
        success = (fnew < fcur) ? 1 : 0;
        out->success_hist[out->hist_len] = (double)success;
        ++out->hist_len;

        if (success) {
            memcpy(x, trial, (size_t)dim * sizeof(double));
            fcur = fnew;
            if (fcur < out->best_f) {
                out->best_f = fcur;
                memcpy(out->best_x, x, (size_t)dim * sizeof(double));
            }
        }

        /* 滑动窗口成功率 → 1/5 乘性步长 */
        if (win_fill == window)
            n_succ_win -= (int)win[win_pos];
        else
            ++win_fill;
        win[win_pos] = (double)success;
        n_succ_win += success;
        win_pos = (win_pos + 1) % window;
        p_hat = (double)n_succ_win / (double)win_fill;
        if (win_fill >= window / 2) {
            if (p_hat > p_target + 1e-12)
                sigma *= factor;
            else if (p_hat < p_target - 1e-12)
                sigma /= factor;
        }
        if (sigma < 1e-16) sigma = 1e-16;
        if (sigma > 1e6) sigma = 1e6;
    }
    out->sigma_final = sigma;
    out->gen_used = max_gen;
    {
        int half = out->hist_len / 2;
        int i, nw = 0, nl = 0;
        double sw = 0.0, sl = 0.0;
        for (i = 0; i < half; ++i) {
            sw += out->success_hist[i];
            ++nw;
        }
        for (i = half; i < out->hist_len; ++i) {
            sl += out->success_hist[i];
            ++nl;
        }
        out->success_rate_warm = nw > 0 ? sw / nw : 0.0;
        out->success_rate_late = nl > 0 ? sl / nl : 0.0;
    }
    free(x); free(trial); free(win);
    return 1;
}

/* ---------- (μ/ρ,λ)-ES ---------- */

void mlab_es_murl_result_free(mlab_es_murl_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->pop_best_hist);
    free(r->sigma_hist);
    r->best_x = NULL;
    r->pop_best_hist = NULL;
    r->sigma_hist = NULL;
}

static void murl_sort_asc(double *fit, int *idx, int n)
{
    int i, j;
    for (i = 1; i < n; ++i) {
        double fi = fit[i];
        int ii = idx[i];
        j = i - 1;
        while (j >= 0 && fit[j] > fi) {
            fit[j + 1] = fit[j];
            idx[j + 1] = idx[j];
            --j;
        }
        fit[j + 1] = fi;
        idx[j + 1] = ii;
    }
}

int mlab_es_murl_run(mlab_rng *rng, const mlab_es_murl_config *cfg,
                     mlab_es_murl_result *out)
{
    int dim, mu, rho, lam, g, i, k, d, max_gen, plus;
    double *X, *S, *fit;         /* 父代 */
    double *OX, *OS, *ofit;      /* 子代 */
    int *oidx;
    double tau, scale;
    int sel_idx_buf[64];

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0)
        return 0;
    mu = cfg->mu > 0 ? cfg->mu : 1;
    rho = cfg->rho > 0 ? cfg->rho : 1;
    lam = cfg->lambda_n > 0 ? cfg->lambda_n : 10 * mu;
    plus = cfg->plus ? 1 : 0;
    if (rho > mu) rho = mu;
    if (rho < 1) rho = 1;
    if (mu > 64) return 0; /* 重组索引缓冲为固定栈数组 */
    if (!plus && lam < mu) return 0; /* comma 选择需要 λ ≥ μ */
    dim = cfg->dim;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 200;
    tau = cfg->tau > 0.0 ? cfg->tau : 1.0 / sqrt(2.0 * (double)dim);
    scale = cfg->x0_scale > 0.0 ? cfg->x0_scale : 2.0;

    memset(out, 0, sizeof *out);
    X = (double *)malloc((size_t)mu * dim * sizeof(double));
    S = (double *)malloc((size_t)mu * sizeof(double));
    fit = (double *)malloc((size_t)mu * sizeof(double));
    OX = (double *)malloc((size_t)lam * dim * sizeof(double));
    OS = (double *)malloc((size_t)lam * sizeof(double));
    ofit = (double *)malloc((size_t)lam * sizeof(double));
    oidx = (int *)malloc((size_t)lam * sizeof(int));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->pop_best_hist = (double *)malloc((size_t)(max_gen + 1) * sizeof(double));
    out->sigma_hist = (double *)malloc((size_t)(max_gen + 1) * sizeof(double));
    if (!X || !S || !fit || !OX || !OS || !ofit || !oidx || !out->best_x ||
        !out->pop_best_hist || !out->sigma_hist) {
        free(X); free(S); free(fit); free(OX); free(OS); free(ofit); free(oidx);
        mlab_es_murl_result_free(out);
        return 0;
    }

    /* 初始父代：盒内均匀随机 */
    for (i = 0; i < mu; ++i) {
        for (d = 0; d < dim; ++d)
            X[i * dim + d] = (2.0 * mlab_rng_uniform(rng) - 1.0) * scale;
        if (cfg->lb && cfg->ub) {
            for (d = 0; d < dim; ++d) {
                if (X[i * dim + d] < cfg->lb[d]) X[i * dim + d] = cfg->lb[d];
                if (X[i * dim + d] > cfg->ub[d]) X[i * dim + d] = cfg->ub[d];
            }
        }
        S[i] = cfg->sigma0 > 0.0 ? cfg->sigma0 : 1.0;
        fit[i] = cfg->f(&X[i * dim], dim, cfg->ctx);
        ++out->n_eval;
    }
    {
        int bi = 0;
        for (i = 1; i < mu; ++i)
            if (fit[i] < fit[bi]) bi = i;
        out->best_f = fit[bi];
        memcpy(out->best_x, &X[bi * dim], (size_t)dim * sizeof(double));
        out->pop_best_hist[out->hist_len] = fit[bi];
        out->sigma_hist[out->hist_len] = S[bi];
        ++out->hist_len;
    }

    for (g = 0; g < max_gen; ++g) {
        /* λ 个子代：ρ 父中间重组 + log-normal σ 变异 */
        for (k = 0; k < lam; ++k) {
            int j;
            double sm = 0.0, sig_k;
            for (j = 0; j < mu; ++j) sel_idx_buf[j] = j;
            /* 部分 Fisher-Yates：取 ρ 个不同父代 */
            for (j = 0; j < rho; ++j) {
                int t = j + (int)(mlab_rng_uniform(rng) * (double)(mu - j));
                int tmp_i;
                if (t >= mu) t = mu - 1;
                tmp_i = sel_idx_buf[j];
                sel_idx_buf[j] = sel_idx_buf[t];
                sel_idx_buf[t] = tmp_i;
            }
            for (j = 0; j < rho; ++j) {
                int pi = sel_idx_buf[j];
                sm += S[pi];
            }
            sm /= (double)rho;
            sig_k = sm * exp(tau * mlab_rng_normal(rng));
            if (sig_k < 1e-16) sig_k = 1e-16;
            if (sig_k > 1e6) sig_k = 1e6;
            for (d = 0; d < dim; ++d) {
                double xm = 0.0;
                for (j = 0; j < rho; ++j) {
                    int pi = sel_idx_buf[j];
                    xm += X[pi * dim + d];
                }
                xm /= (double)rho;
                OX[k * dim + d] = xm + sig_k * mlab_rng_normal(rng);
                if (cfg->lb && cfg->ub) {
                    if (OX[k * dim + d] < cfg->lb[d]) OX[k * dim + d] = cfg->lb[d];
                    if (OX[k * dim + d] > cfg->ub[d]) OX[k * dim + d] = cfg->ub[d];
                }
            }
            OS[k] = sig_k;
            ofit[k] = cfg->f(&OX[k * dim], dim, cfg->ctx);
            ++out->n_eval;
            if (ofit[k] < out->best_f) {
                out->best_f = ofit[k];
                memcpy(out->best_x, &OX[k * dim], (size_t)dim * sizeof(double));
            }
        }

        /* 选择：comma 取 λ 中最好 μ；plus 取父代∪子代中最好 μ */
        if (!plus) {
            for (k = 0; k < lam; ++k) oidx[k] = k;
            murl_sort_asc(ofit, oidx, lam);
            for (i = 0; i < mu; ++i) {
                int ii = oidx[i];
                fit[i] = ofit[ii];
                S[i] = OS[ii];
                memcpy(&X[i * dim], &OX[ii * dim], (size_t)dim * sizeof(double));
            }
        } else {
            /* 合并排序：父代 [0,mu) + 子代 [mu, mu+lam) */
            {
                int n_all = mu + lam;
                double *afit = (double *)malloc((size_t)n_all * sizeof(double));
                int *aidx = (int *)malloc((size_t)n_all * sizeof(int));
                double *nX = (double *)malloc((size_t)mu * dim * sizeof(double));
                double *nS = (double *)malloc((size_t)mu * sizeof(double));
                double *nfit = (double *)malloc((size_t)mu * sizeof(double));
                if (!afit || !aidx || !nX || !nS || !nfit) {
                    free(afit); free(aidx); free(nX); free(nS); free(nfit);
                    mlab_es_murl_result_free(out);
                    return 0;
                }
                for (i = 0; i < mu; ++i) {
                    afit[i] = fit[i];
                    aidx[i] = i;
                }
                for (k = 0; k < lam; ++k) {
                    afit[mu + k] = ofit[k];
                    aidx[mu + k] = mu + k;
                }
                murl_sort_asc(afit, aidx, n_all);
                for (i = 0; i < mu; ++i) {
                    int ai = aidx[i];
                    nfit[i] = afit[i];
                    if (ai < mu) {
                        nS[i] = S[ai];
                        memcpy(&nX[i * dim], &X[ai * dim], (size_t)dim * sizeof(double));
                    } else {
                        int k2 = ai - mu;
                        nS[i] = OS[k2];
                        memcpy(&nX[i * dim], &OX[k2 * dim], (size_t)dim * sizeof(double));
                    }
                }
                memcpy(X, nX, (size_t)mu * dim * sizeof(double));
                memcpy(S, nS, (size_t)mu * sizeof(double));
                memcpy(fit, nfit, (size_t)mu * sizeof(double));
                free(afit); free(aidx); free(nX); free(nS); free(nfit);
            }
        }
        {
            int bi = 0;
            for (i = 1; i < mu; ++i)
                if (fit[i] < fit[bi]) bi = i;
            out->pop_best_hist[out->hist_len] = fit[bi];
            out->sigma_hist[out->hist_len] = S[bi];
            ++out->hist_len;
            out->sigma_final = S[bi];
        }
        out->gen_used = g + 1;
    }
    free(X); free(S); free(fit); free(OX); free(OS); free(ofit); free(oidx);
    return 1;
}
