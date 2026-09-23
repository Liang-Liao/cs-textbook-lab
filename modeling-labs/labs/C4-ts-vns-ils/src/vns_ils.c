#include "vns_ils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_vns_result_free(mlab_vns_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

void mlab_ils_result_free(mlab_ils_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

int mlab_tsp_first_improve(const mlab_tsp_inst *inst, int *tour, double *len,
                           unsigned nbhd_mask, int max_evals, int *fevals)
{
    int n = inst->n;
    int improved = 0;
    int *cand;
    unsigned kinds[3];
    int nk = 0, ki;
    int used = 0; /* 本次调用内的评估计数（预算按调用算，不比全局累计值） */

    if (nbhd_mask & MLAB_NB_SWAP) kinds[nk++] = MLAB_NB_SWAP;
    if (nbhd_mask & MLAB_NB_2OPT) kinds[nk++] = MLAB_NB_2OPT;
    if (nbhd_mask & MLAB_NB_OROPT) kinds[nk++] = MLAB_NB_OROPT;
    if (nk == 0) return 0;
    cand = (int *)malloc((size_t)n * sizeof(int));
    if (!cand) return 0;

    for (ki = 0; ki < nk; ++ki) {
        int i, j;
        unsigned kind = kinds[ki];
        int found = 0;
        for (i = 0; i < n && !found; ++i) {
            for (j = 0; j < n; ++j) {
                double flen;
                if (kind != MLAB_NB_OROPT && j <= i) continue;
                if (kind == MLAB_NB_OROPT && i == j) continue;
                if (max_evals > 0 && used >= max_evals) break;
                memcpy(cand, tour, (size_t)n * sizeof(int));
                if (kind == MLAB_NB_2OPT)
                    mlab_tsp_apply_2opt(cand, n, i, j);
                else if (kind == MLAB_NB_OROPT)
                    mlab_tsp_apply_oropt(cand, n, i, j);
                else
                    mlab_tsp_apply_swap(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                ++(*fevals);
                ++used;
                if (flen < *len - 1e-12) {
                    memcpy(tour, cand, (size_t)n * sizeof(int));
                    *len = flen;
                    improved = 1;
                    found = 1;
                    break;
                }
            }
        }
    }
    free(cand);
    return improved;
}

int mlab_tsp_best_improve(const mlab_tsp_inst *inst, int *tour, double *len,
                          unsigned nbhd_mask, int max_evals, int *fevals)
{
    int n = inst->n;
    int improved = 0;
    int *cand, *best;
    unsigned kinds[3];
    int nk = 0, ki;
    int used = 0;
    double best_flen = 1e300;
    int bi = -1;

    if (nbhd_mask & MLAB_NB_SWAP) kinds[nk++] = MLAB_NB_SWAP;
    if (nbhd_mask & MLAB_NB_2OPT) kinds[nk++] = MLAB_NB_2OPT;
    if (nbhd_mask & MLAB_NB_OROPT) kinds[nk++] = MLAB_NB_OROPT;
    if (nk == 0) return 0;
    cand = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    if (!cand || !best) {
        free(cand);
        free(best);
        return 0;
    }

    for (ki = 0; ki < nk; ++ki) {
        int i, j;
        unsigned kind = kinds[ki];
        for (i = 0; i < n; ++i) {
            for (j = 0; j < n; ++j) {
                double flen;
                if (kind != MLAB_NB_OROPT && j <= i) continue;
                if (kind == MLAB_NB_OROPT && i == j) continue;
                if (max_evals > 0 && used >= max_evals) goto done;
                memcpy(cand, tour, (size_t)n * sizeof(int));
                if (kind == MLAB_NB_2OPT)
                    mlab_tsp_apply_2opt(cand, n, i, j);
                else if (kind == MLAB_NB_OROPT)
                    mlab_tsp_apply_oropt(cand, n, i, j);
                else
                    mlab_tsp_apply_swap(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                ++(*fevals);
                ++used;
                if (flen < best_flen) {
                    best_flen = flen;
                    bi = i;
                    memcpy(best, cand, (size_t)n * sizeof(int));
                }
            }
        }
    }
done:
    if (bi >= 0 && best_flen < *len - 1e-12) {
        memcpy(tour, best, (size_t)n * sizeof(int));
        *len = best_flen;
        improved = 1;
    }
    free(cand);
    free(best);
    return improved;
}

int mlab_tsp_vnd(const mlab_tsp_inst *inst, int *tour, double *len,
                 unsigned nbhd_mask, int max_evals, int *fevals)
{
    unsigned order[3];
    int nk = 0, k = 0, any = 0;
    int used = 0; /* 本次 VND 调用内消耗（由 *fevals 差分记账） */
    /* 邻域按价值排序：2-opt 主力先行，swap 扫描贵且弱、仅作收尾，
     * 避免每次改进后从弱邻域重扫造成的预算稀释 */
    if (nbhd_mask & MLAB_NB_2OPT) order[nk++] = MLAB_NB_2OPT;
    if (nbhd_mask & MLAB_NB_OROPT) order[nk++] = MLAB_NB_OROPT;
    if (nbhd_mask & MLAB_NB_SWAP) order[nk++] = MLAB_NB_SWAP;
    if (nk == 0) return 0;
    while (k < nk) {
        int before = *fevals;
        int rem = max_evals > 0 ? max_evals - used : 0;
        if (mlab_tsp_first_improve(inst, tour, len, order[k], rem, fevals)) {
            any = 1;
            k = 0;
        } else {
            ++k;
        }
        used += *fevals - before;
        if (max_evals > 0 && used >= max_evals) break;
    }
    return any;
}

static void apply_nbhd_move(int *tour, int n, unsigned kind, int a, int b)
{
    if (kind == MLAB_NB_2OPT)
        mlab_tsp_apply_2opt(tour, n, a, b);
    else if (kind == MLAB_NB_OROPT)
        mlab_tsp_apply_oropt(tour, n, a, b);
    else
        mlab_tsp_apply_swap(tour, n, a, b);
}

static void shake(mlab_rng *rng, int *tour, int n, unsigned mask, int k)
{
    unsigned kinds[3];
    int nk = 0, s;
    if (mask & MLAB_NB_SWAP) kinds[nk++] = MLAB_NB_SWAP;
    if (mask & MLAB_NB_2OPT) kinds[nk++] = MLAB_NB_2OPT;
    if (mask & MLAB_NB_OROPT) kinds[nk++] = MLAB_NB_OROPT;
    if (nk == 0) return;
    for (s = 0; s < k; ++s) {
        unsigned kind = kinds[(int)(mlab_rng_uniform(rng) * nk) % nk];
        int a = (int)(mlab_rng_uniform(rng) * n);
        int b = (int)(mlab_rng_uniform(rng) * n);
        if (a == b) b = (b + 1) % n;
        if (b >= n) b = 0;
        apply_nbhd_move(tour, n, kind, a, b);
    }
}

int mlab_vns_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_vns_config *cfg,
                 const int *tour0,
                 mlab_vns_result *out)
{
    int n, *cur, *best, *work;
    double f_cur, f_best;
    int fevals = 0, outer = 0, k, kmax;
    int max_fe, max_outer;

    if (!rng || !inst || !cfg || !tour0 || !out || inst->n < 4) return 0;
    if (cfg->nbhd_mask == 0) return 0;
    n = inst->n;
    memset(out, 0, sizeof *out);
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    work = (int *)malloc((size_t)n * sizeof(int));
    if (!cur || !best || !work) {
        free(cur); free(best); free(work);
        return 0;
    }
    memcpy(cur, tour0, (size_t)n * sizeof(int));
    memcpy(best, tour0, (size_t)n * sizeof(int));
    f_cur = mlab_tsp_length(inst, cur);
    f_best = f_cur;
    fevals = 1;
    out->best_tour = best;

    max_fe = cfg->max_fevals > 0 ? cfg->max_fevals : 5000;
    max_outer = cfg->max_outer > 0 ? cfg->max_outer : max_fe;
    kmax = cfg->kmax > 0 ? cfg->kmax : 5;
    k = 1;

    while (fevals < max_fe && outer < max_outer) {
        int fe_before = fevals;
        int ls_cap;
        double f_work;
        memcpy(work, cur, (size_t)n * sizeof(int));
        shake(rng, work, n, cfg->nbhd_mask, k);
        f_work = mlab_tsp_length(inst, work);
        ++fevals;
        /* 单次局部搜索预算 = min(ls_pass_limit, 剩余全局预算) */
        ls_cap = max_fe - fevals;
        if (ls_cap < 0) ls_cap = 0;
        if (cfg->ls_pass_limit > 0 && ls_cap > cfg->ls_pass_limit)
            ls_cap = cfg->ls_pass_limit;
        if (cfg->vnd)
            mlab_tsp_vnd(inst, work, &f_work, cfg->nbhd_mask,
                         ls_cap, &fevals);
        else {
            while (fevals < max_fe && ls_cap > 0 &&
                   mlab_tsp_first_improve(inst, work, &f_work, cfg->nbhd_mask,
                                          ls_cap, &fevals)) {
                ls_cap = max_fe - fevals;
                if (ls_cap < 0) ls_cap = 0;
                if (cfg->ls_pass_limit > 0 && ls_cap > cfg->ls_pass_limit)
                    ls_cap = cfg->ls_pass_limit;
            }
        }
        ++out->n_local;
        if (f_work < f_cur - 1e-12) {
            memcpy(cur, work, (size_t)n * sizeof(int));
            f_cur = f_work;
            k = 1;
            if (f_cur < f_best) {
                f_best = f_cur;
                memcpy(best, cur, (size_t)n * sizeof(int));
            }
        } else if (k < kmax) {
            ++k;
        }
        ++outer;
        if (fevals == fe_before) ++fevals;
    }
    out->best_len = f_best;
    out->n_fevals = fevals;
    out->n_outer = outer;
    free(cur);
    free(work);
    return 1;
}

int mlab_ils_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_ils_config *cfg,
                 const int *tour0,
                 mlab_ils_result *out)
{
    int n, *cur, *best, *work;
    double f_cur, f_best;
    int fevals = 0, outer = 0;
    int max_fe, max_outer, s;
    unsigned mask;

    if (!rng || !inst || !cfg || !tour0 || !out || inst->n < 4) return 0;
    n = inst->n;
    memset(out, 0, sizeof *out);
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    work = (int *)malloc((size_t)n * sizeof(int));
    if (!cur || !best || !work) {
        free(cur); free(best); free(work);
        return 0;
    }
    mask = cfg->nbhd_mask ? cfg->nbhd_mask : MLAB_NB_ALL;
    max_fe = cfg->max_fevals > 0 ? cfg->max_fevals : 5000;
    memcpy(cur, tour0, (size_t)n * sizeof(int));
    f_cur = mlab_tsp_length(inst, cur);
    ++fevals;
    /* 初始局部搜索：上限 = min(ls_pass_limit, 剩余预算) */
    {
        int cap0 = max_fe - fevals;
        if (cap0 < 0) cap0 = 0;
        if (cfg->ls_pass_limit > 0 && cap0 > cfg->ls_pass_limit)
            cap0 = cfg->ls_pass_limit;
        mlab_tsp_vnd(inst, cur, &f_cur, mask, cap0, &fevals);
    }
    f_best = f_cur;
    memcpy(best, cur, (size_t)n * sizeof(int));
    out->best_tour = best;

    max_outer = cfg->max_outer > 0 ? cfg->max_outer : max_fe;
    s = cfg->pert_strength; /* 允许 0：无扰动对照 */
    if (s < 0) s = 0;

    while (fevals < max_fe && outer < max_outer) {
        int fe_before = fevals;
        int ls_cap;
        double f_work;
        memcpy(work, cur, (size_t)n * sizeof(int));
        shake(rng, work, n, MLAB_NB_ALL, s);
        f_work = mlab_tsp_length(inst, work);
        ++fevals;
        ls_cap = max_fe - fevals;
        if (ls_cap < 0) ls_cap = 0;
        if (cfg->ls_pass_limit > 0 && ls_cap > cfg->ls_pass_limit)
            ls_cap = cfg->ls_pass_limit;
        mlab_tsp_vnd(inst, work, &f_work, mask, ls_cap, &fevals);
        ++out->n_outer;
        if (f_work < f_cur - 1e-12 ||
            (cfg->accept_worse && mlab_rng_uniform(rng) < 0.05)) {
            memcpy(cur, work, (size_t)n * sizeof(int));
            f_cur = f_work;
            ++out->n_accept;
            if (f_cur < f_best) {
                f_best = f_cur;
                memcpy(best, cur, (size_t)n * sizeof(int));
            }
        }
        if (fevals == fe_before) ++fevals;
        ++outer;
    }
    out->best_len = f_best;
    out->n_fevals = fevals;
    free(cur);
    free(work);
    return 1;
}
