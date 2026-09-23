#include "moead.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_moead_result_free(mlab_moead_result *r)
{
    if (!r) return;
    free(r->X);
    free(r->F);
    free(r->igd_hist);
    r->X = NULL;
    r->F = NULL;
    r->igd_hist = NULL;
}

static void sbx_mut_one(mlab_rng *rng, const double *p1, const double *p2,
                        double *c, int dim, const double *lb, const double *ub,
                        double p_cross, double eta_c, double p_mut, double eta_m)
{
    int d;
    for (d = 0; d < dim; ++d) {
        double y1 = p1[d], y2 = p2[d];
        if (mlab_rng_uniform(rng) < p_cross) {
            double u = mlab_rng_uniform(rng);
            double beta;
            if (u <= 0.5)
                beta = pow(2.0 * u, 1.0 / (eta_c + 1.0));
            else
                beta = pow(1.0 / (2.0 * (1.0 - u + 1e-16)), 1.0 / (eta_c + 1.0));
            c[d] = 0.5 * ((1.0 + beta) * y1 + (1.0 - beta) * y2);
        } else {
            c[d] = y1;
        }
        if (mlab_rng_uniform(rng) < p_mut) {
            double lo = lb ? lb[d] : 0.0;
            double hi = ub ? ub[d] : 1.0;
            double y = c[d];
            double u = mlab_rng_uniform(rng);
            if (u < 0.5) {
                double delta = pow(2.0 * u, 1.0 / (eta_m + 1.0)) - 1.0;
                y = y + delta * (y - lo);
            } else {
                double delta = 1.0 - pow(2.0 * (1.0 - u), 1.0 / (eta_m + 1.0));
                y = y + delta * (hi - y);
            }
            c[d] = y;
        }
        if (lb && c[d] < lb[d]) c[d] = lb[d];
        if (ub && c[d] > ub[d]) c[d] = ub[d];
    }
}

int mlab_moead_run(mlab_rng *rng, const mlab_moead_config *cfg,
                   const double *igd_ref, int nref, mlab_moead_result *out)
{
    int dim, np, T, g, i, j, d, nobj;
    double *X, *F, *lam, *z, *child, *cF;
    int *nb; /* pop * T 邻域表 */
    double p_cross, eta_c, p_mut, eta_m;

    if (!rng || !cfg || !cfg->eval || !out || cfg->dim <= 0 || cfg->pop < 5)
        return 0;
    if (cfg->nobj != 2) return 0; /* 最小实现仅 2 目标 */
    dim = cfg->dim;
    np = cfg->pop;
    nobj = 2;
    T = cfg->T > 1 ? cfg->T : 5;
    if (T > np - 1) T = np - 1;
    p_cross = cfg->p_cross > 0 ? cfg->p_cross : 0.9;
    eta_c = cfg->eta_c > 0 ? cfg->eta_c : 20.0;
    p_mut = cfg->p_mut > 0 ? cfg->p_mut : 1.0 / (double)dim;
    eta_m = cfg->eta_m > 0 ? cfg->eta_m : 20.0;

    memset(out, 0, sizeof *out);
    out->pop = np;
    out->dim = dim;
    out->nobj = nobj;

    X = (double *)malloc((size_t)np * dim * sizeof(double));
    F = (double *)malloc((size_t)np * nobj * sizeof(double));
    lam = (double *)malloc((size_t)np * nobj * sizeof(double));
    z = (double *)malloc((size_t)nobj * sizeof(double));
    child = (double *)malloc((size_t)dim * sizeof(double));
    cF = (double *)malloc((size_t)nobj * sizeof(double));
    nb = (int *)malloc((size_t)np * T * sizeof(int));
    out->X = (double *)malloc((size_t)np * dim * sizeof(double));
    out->F = (double *)malloc((size_t)np * nobj * sizeof(double));
    out->igd_hist = (double *)malloc((size_t)(cfg->max_gen + 2) * sizeof(double));
    if (!X || !F || !lam || !z || !child || !cF || !nb ||
        !out->X || !out->F || !out->igd_hist) {
        free(X); free(F); free(lam); free(z); free(child); free(cF);
        free(nb);
        mlab_moead_result_free(out);
        return 0;
    }

    /* 均匀权重：(λ1,λ2) = (k/(K-1), 1-k/(K-1)) */
    for (i = 0; i < np; ++i) {
        double a = (np > 1) ? (double)i / (double)(np - 1) : 0.0;
        lam[i * nobj] = a;
        lam[i * nobj + 1] = 1.0 - a;
    }
    /* 邻域：按权重欧氏距离最近的 T 个（含自身，自身距离 0 必入选） */
    for (i = 0; i < np; ++i) {
        double *dist = (double *)malloc((size_t)np * sizeof(double));
        int *idx = (int *)malloc((size_t)np * sizeof(int));
        int a, b;
        if (!dist || !idx) {
            free(dist); free(idx);
            free(X); free(F); free(lam); free(z); free(child); free(cF);
            free(nb);
            mlab_moead_result_free(out);
            return 0;
        }
        for (j = 0; j < np; ++j) {
            double d1 = lam[i * nobj] - lam[j * nobj];
            double d2 = lam[i * nobj + 1] - lam[j * nobj + 1];
            dist[j] = sqrt(d1 * d1 + d2 * d2);
            idx[j] = j;
        }
        /* 插入排序取前 T（np 小，足够） */
        for (a = 1; a < np; ++a) {
            double da = dist[a];
            int ia = idx[a];
            b = a - 1;
            while (b >= 0 && dist[b] > da) {
                dist[b + 1] = dist[b];
                idx[b + 1] = idx[b];
                --b;
            }
            dist[b + 1] = da;
            idx[b + 1] = ia;
        }
        for (a = 0; a < T; ++a) nb[i * T + a] = idx[a];
        free(dist); free(idx);
    }

    /* 初始种群 + 理想点 */
    z[0] = z[1] = 1e300;
    for (i = 0; i < np; ++i) {
        for (d = 0; d < dim; ++d) {
            double lo = cfg->lb ? cfg->lb[d] : 0.0;
            double hi = cfg->ub ? cfg->ub[d] : 1.0;
            X[i * dim + d] = lo + (hi - lo) * mlab_rng_uniform(rng);
        }
        cfg->eval(&X[i * dim], dim, &F[i * nobj], nobj, cfg->ctx);
        ++out->n_eval;
        if (F[i * nobj] < z[0]) z[0] = F[i * nobj];
        if (F[i * nobj + 1] < z[1]) z[1] = F[i * nobj + 1];
    }

    {
        double igd0 = igd_ref ? mlab_igd(igd_ref, nref, F, np, nobj) : 0.0;
        if (igd_ref) out->igd_hist[out->igd_len++] = igd0;
    }

    for (g = 0; g < cfg->max_gen; ++g) {
        double p_global = cfg->p_mate_global > 0.0 ? cfg->p_mate_global : 0.1;
        for (i = 0; i < np; ++i) {
            int r1, r2;
            /* 父代选择：δ 概率全局（探索），否则邻域内（开发）——标准 MOEA/D */
            if (mlab_rng_uniform(rng) < p_global) {
                r1 = (int)(mlab_rng_uniform(rng) * np) % np;
                do {
                    r2 = (int)(mlab_rng_uniform(rng) * np) % np;
                } while (r2 == r1);
            } else {
                r1 = nb[i * T + (int)(mlab_rng_uniform(rng) * T) % T];
                do {
                    r2 = nb[i * T + (int)(mlab_rng_uniform(rng) * T) % T];
                } while (r2 == r1);
            }
            /* 子代：DE/rand/1/bin（与 nsga2 同款 C5 机制，全变量差分）
             * + 10% 概率再叠 SBX/多项式扰动。MOEA/D-DE 风格：
             * 纯 SBX/PM 只能逐维研磨 g(x)，多变量同步差分是收敛关键 */
            {
                int r3, jrand;
                double Fde = 0.5, CRde = 0.9;
                do {
                    r3 = mlab_rng_uniform(rng) < p_global
                             ? (int)(mlab_rng_uniform(rng) * np) % np
                             : nb[i * T + (int)(mlab_rng_uniform(rng) * T) % T];
                } while (r3 == r1 || r3 == r2);
                jrand = (int)(mlab_rng_uniform(rng) * dim) % dim;
                for (d = 0; d < dim; ++d) {
                    double v = X[r1 * dim + d] +
                               Fde * (X[r2 * dim + d] - X[r3 * dim + d]);
                    if (!(mlab_rng_uniform(rng) < CRde) && d != jrand)
                        v = X[i * dim + d];
                    if (cfg->lb && v < cfg->lb[d]) v = cfg->lb[d];
                    if (cfg->ub && v > cfg->ub[d]) v = cfg->ub[d];
                    child[d] = v;
                }
                if (mlab_rng_uniform(rng) < 0.10)
                    sbx_mut_one(rng, &X[r1 * dim], &X[r2 * dim], child, dim,
                                cfg->lb, cfg->ub, p_cross, eta_c, p_mut, eta_m);
            }
            cfg->eval(child, dim, cF, nobj, cfg->ctx);
            ++out->n_eval;
            if (cF[0] < z[0]) z[0] = cF[0];
            if (cF[1] < z[1]) z[1] = cF[1];
            /* Tchebycheff：g(x|λ_j, z) = max(λ1|f1-z1|, λ2|f2-z2|)；
             * 对子问题 i 及其邻域做（严格）替换尝试 */
            for (j = 0; j < T; ++j) {
                int k = nb[i * T + j];
                double g_child, g_cur;
                double a1 = lam[k * nobj] * fabs(cF[0] - z[0]);
                double a2 = lam[k * nobj + 1] * fabs(cF[1] - z[1]);
                double b1 = lam[k * nobj] * fabs(F[k * nobj] - z[0]);
                double b2 = lam[k * nobj + 1] * fabs(F[k * nobj + 1] - z[1]);
                g_child = a1 > a2 ? a1 : a2;
                g_cur = b1 > b2 ? b1 : b2;
                if (g_child < g_cur) {
                    memcpy(&X[k * dim], child, (size_t)dim * sizeof(double));
                    F[k * nobj] = cF[0];
                    F[k * nobj + 1] = cF[1];
                }
            }
        }
        out->gen_used = g + 1;
        if (igd_ref && out->igd_len < cfg->max_gen + 2) {
            out->igd_hist[out->igd_len++] =
                mlab_igd(igd_ref, nref, F, np, nobj);
        }
    }
    memcpy(out->X, X, (size_t)np * dim * sizeof(double));
    memcpy(out->F, F, (size_t)np * nobj * sizeof(double));
    free(X); free(F); free(lam); free(z); free(child); free(cF);
    free(nb);
    return 1;
}
