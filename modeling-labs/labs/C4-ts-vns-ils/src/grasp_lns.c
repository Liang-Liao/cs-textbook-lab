#include "grasp_lns.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- LNS ---- */

void mlab_lns_result_free(mlab_lns_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

static double inst_dist(const mlab_tsp_inst *inst, int a, int b)
{
    int n = inst->n;
    if (inst->dist) return inst->dist[a * n + b];
    {
        double dx = inst->x[a] - inst->x[b];
        double dy = inst->y[a] - inst->y[b];
        return sqrt(dx * dx + dy * dy);
    }
}

/*
 * 逐城最小代价重插（removed 顺序即重插顺序）。
 * 位置代价 Δ = d(prev,c)+d(c,next)−d(prev,next)，O(1)/位置。
 * 评估记账：每重插一城 ≈ 一次 O(n) 边扫描，计 1 次评估。
 */
static void repair_cheapest(const mlab_tsp_inst *inst, int *tour, int n,
                            const int *removed, int k)
{
    int len = n - k, r, pos, i;
    for (r = 0; r < k; ++r) {
        int c = removed[r];
        double best_delta = 1e300;
        int best_pos = 0;
        for (pos = 0; pos <= len; ++pos) {
            int prev = tour[(pos - 1 + len) % len];
            int next = tour[pos % len];
            double delta;
            if (len == 0) {
                delta = 0.0; /* 空回路（k<=n-3 时不会出现） */
            } else {
                delta = inst_dist(inst, prev, c) +
                        inst_dist(inst, c, next) -
                        inst_dist(inst, prev, next);
            }
            if (delta < best_delta) {
                best_delta = delta;
                best_pos = pos;
            }
        }
        for (i = len; i > best_pos; --i) tour[i] = tour[i - 1];
        tour[best_pos] = c;
        ++len;
    }
}

int mlab_lns_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_lns_config *cfg,
                 const int *tour0,
                 mlab_lns_result *out)
{
    int n, *cur, *best, *work, *removed;
    unsigned char *rm;
    double f_cur, f_best;
    int fevals = 0, outer = 0, k;
    int max_fe, max_outer;

    if (!rng || !inst || !cfg || !tour0 || !out || inst->n < 4) return 0;
    k = cfg->destroy_count;
    if (k < 1 || k > inst->n - 3) return 0;
    n = inst->n;
    memset(out, 0, sizeof *out);
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    work = (int *)malloc((size_t)n * sizeof(int));
    removed = (int *)malloc((size_t)k * sizeof(int));
    rm = (unsigned char *)calloc((size_t)n, 1);
    if (!cur || !best || !work || !removed || !rm) {
        free(cur); free(best); free(work); free(removed); free(rm);
        return 0;
    }
    memcpy(cur, tour0, (size_t)n * sizeof(int));
    f_cur = mlab_tsp_length(inst, cur);
    ++fevals;
    f_best = f_cur;
    memcpy(best, cur, (size_t)n * sizeof(int));
    out->best_tour = best;

    max_fe = cfg->max_fevals > 0 ? cfg->max_fevals : 5000;
    max_outer = cfg->max_outer > 0 ? cfg->max_outer : max_fe;

    while (fevals < max_fe && outer < max_outer) {
        int i, cnt = 0, w = 0;
        double f_work;
        /* 毁灭：不放回随机抽 k 城（拒绝采样，k<=n-3 保证终止） */
        memset(rm, 0, (size_t)n);
        while (cnt < k) {
            int c = (int)(mlab_rng_uniform(rng) * n);
            if (c < 0) c = 0;
            if (c >= n) c = n - 1;
            if (!rm[c]) {
                rm[c] = 1;
                removed[cnt++] = c;
            }
        }
        /* 保留序列（相对顺序不变）放入 work[0..n-k-1] */
        for (i = 0; i < n; ++i)
            if (!rm[i]) work[w++] = cur[i];
        /* 重建：最小代价逐城重插 */
        repair_cheapest(inst, work, n, removed, k);
        fevals += k;
        f_work = mlab_tsp_length(inst, work);
        ++fevals;
        if (cfg->nbhd_mask) {
            int cap = max_fe - fevals;
            if (cap < 0) cap = 0;
            if (cfg->ls_pass_limit > 0 && cap > cfg->ls_pass_limit)
                cap = cfg->ls_pass_limit;
            mlab_tsp_vnd(inst, work, &f_work, cfg->nbhd_mask, cap, &fevals);
        }
        if (f_work < f_cur - 1e-12) {
            memcpy(cur, work, (size_t)n * sizeof(int));
            f_cur = f_work;
            ++out->n_accept;
            if (f_cur < f_best) {
                f_best = f_cur;
                memcpy(best, cur, (size_t)n * sizeof(int));
            }
        }
        ++outer;
    }
    out->best_len = f_best;
    out->n_fevals = fevals;
    out->n_outer = outer;
    free(cur);
    free(work);
    free(removed);
    free(rm);
    return 1;
}

/* ---- GRASP ---- */

void mlab_grasp_result_free(mlab_grasp_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

/* 受限候选表（RCL）随机贪心 NN 构造 */
static void grasp_construct(mlab_rng *rng, const mlab_tsp_inst *inst,
                            double alpha, int *tour)
{
    int n = inst->n;
    unsigned char *used = (unsigned char *)calloc((size_t)n, 1);
    int start, t, i;
    if (!used) {
        mlab_tsp_identity_tour(n, tour);
        return;
    }
    start = (int)(mlab_rng_uniform(rng) * n);
    if (start >= n) start = n - 1;
    tour[0] = start;
    used[start] = 1;
    for (t = 1; t < n; ++t) {
        int last = tour[t - 1];
        double dmin = 1e300, dmax = 0.0, cut;
        int cnt = 0, pick_idx, acc;
        for (i = 0; i < n; ++i) {
            double d;
            if (used[i]) continue;
            d = inst_dist(inst, last, i);
            if (d < dmin) dmin = d;
            if (d > dmax) dmax = d;
        }
        cut = dmin + alpha * (dmax - dmin);
        for (i = 0; i < n; ++i)
            if (!used[i] && inst_dist(inst, last, i) <= cut + 1e-12) ++cnt;
        if (cnt == 0) { /* 兜底：任取未用城市 */
            for (i = 0; i < n; ++i)
                if (!used[i]) {
                    tour[t] = i;
                    used[i] = 1;
                    break;
                }
            continue;
        }
        pick_idx = (int)(mlab_rng_uniform(rng) * (double)cnt);
        if (pick_idx >= cnt) pick_idx = cnt - 1;
        acc = 0;
        for (i = 0; i < n; ++i) {
            if (used[i]) continue;
            if (inst_dist(inst, last, i) <= cut + 1e-12) {
                if (acc == pick_idx) {
                    tour[t] = i;
                    used[i] = 1;
                    break;
                }
                ++acc;
            }
        }
    }
    free(used);
}

int mlab_grasp_tsp(mlab_rng *rng,
                   const mlab_tsp_inst *inst,
                   const mlab_grasp_config *cfg,
                   mlab_grasp_result *out)
{
    int n, *best, *work;
    double alpha, f_best = 1e300;
    int it, fevals = 0, n_iter;
    int max_fe;
    unsigned mask;

    if (!rng || !inst || !cfg || !out || inst->n < 4) return 0;
    if (cfg->n_iter < 1) return 0;
    n = inst->n;
    memset(out, 0, sizeof *out);
    alpha = cfg->rcl_alpha;
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;
    mask = cfg->nbhd_mask ? cfg->nbhd_mask : MLAB_NB_2OPT;
    max_fe = cfg->max_fevals > 0 ? cfg->max_fevals : 0;
    n_iter = cfg->n_iter;
    best = (int *)malloc((size_t)n * sizeof(int));
    work = (int *)malloc((size_t)n * sizeof(int));
    if (!best || !work) {
        free(best); free(work);
        return 0;
    }
    out->best_tour = best;

    for (it = 1; it <= n_iter; ++it) {
        double f_work;
        int cap;
        grasp_construct(rng, inst, alpha, work);
        f_work = mlab_tsp_length(inst, work);
        ++fevals; /* 每次构造计 1 次评估 */
        if (max_fe > 0 && fevals >= max_fe) break;
        /* 单次局部搜索上限 = min(ls_pass_limit, 剩余预算) */
        cap = max_fe > 0 ? max_fe - fevals : 0;
        if (cfg->ls_pass_limit > 0 && (cap == 0 || cap > cfg->ls_pass_limit))
            cap = cfg->ls_pass_limit;
        if (cap < 0) cap = 0;
        mlab_tsp_vnd(inst, work, &f_work, mask, cap, &fevals);
        if (f_work < f_best) {
            f_best = f_work;
            memcpy(best, work, (size_t)n * sizeof(int));
            out->n_iter = it;
        }
    }
    out->best_len = f_best;
    out->n_fevals = fevals;
    free(work);
    return 1;
}
