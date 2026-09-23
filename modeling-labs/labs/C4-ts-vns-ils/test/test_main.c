/*
 * C4: 禁忌搜索 / VNS / ILS — TSP
 */
#include "harness.h"
#include "lab.h"
#include "tabu.h"
#include "vns_ils.h"
#include "grasp_lns.h"
#include "tsp.h"
#include "sa.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_CITIES 20
#define BUDGET 6000
#define BOX 10.0

static double paired_p_two_sided(const double *a, const double *b, int n)
{
    double mean = 0.0, var = 0.0, t;
    int i;
    if (n < 2) return 1.0;
    for (i = 0; i < n; ++i) mean += a[i] - b[i];
    mean /= n;
    for (i = 0; i < n; ++i) {
        double d = (a[i] - b[i]) - mean;
        var += d * d;
    }
    var /= (n - 1);
    if (var <= 0.0) return mean > 0 ? 0.0 : 1.0;
    t = mean / sqrt(var / n);
    return 2.0 * (1.0 - mlab_normal_cdf(fabs(t), 0.0, 1.0));
}

static double vec_min(const double *x, int n)
{
    double m;
    int k;
    if (n <= 0) return 0;
    m = x[0];
    for (k = 1; k < n; ++k)
        if (x[k] < m) m = x[k];
    return m;
}

/* 带簇结构的实例：局部最优更多，参数差异更明显 */
static int make_cluster_inst(mlab_tsp_inst *inst, int *tour0, unsigned seed)
{
    mlab_rng rng;
    int i, n = N_CITIES;
    memset(inst, 0, sizeof *inst);
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    inst->n = n;
    inst->x = (double *)malloc((size_t)n * sizeof(double));
    inst->y = (double *)malloc((size_t)n * sizeof(double));
    if (!inst->x || !inst->y) {
        mlab_tsp_free(inst);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        int c = i % 4;
        double cx = (c % 2) * 6.0 + 1.0;
        double cy = (c / 2) * 6.0 + 1.0;
        inst->x[i] = cx + mlab_rng_uniform(&rng) * 2.2;
        inst->y[i] = cy + mlab_rng_uniform(&rng) * 2.2;
    }
    mlab_tsp_shuffle_tour(&rng, n, tour0);
    return 1;
}

/* 更难的紧簇实例：n 城 nc 簇（簇内更密 → 2-opt 局部最优更多） */
static int make_hard_cluster_inst(mlab_tsp_inst *inst, int *tour0,
                                  unsigned seed, int n, int nc)
{
    mlab_rng rng;
    int i;
    memset(inst, 0, sizeof *inst);
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    inst->n = n;
    inst->x = (double *)malloc((size_t)n * sizeof(double));
    inst->y = (double *)malloc((size_t)n * sizeof(double));
    if (!inst->x || !inst->y) {
        mlab_tsp_free(inst);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        int c = i % nc;
        double cx = (c % 3) * 4.0 + 1.0;
        double cy = (c / 3) * 4.0 + 1.0;
        inst->x[i] = cx + mlab_rng_uniform(&rng) * 1.4;
        inst->y[i] = cy + mlab_rng_uniform(&rng) * 1.4;
    }
    mlab_tsp_shuffle_tour(&rng, n, tour0);
    return 1;
}

static int u_shape_check(const double *means, int n, int ibest,
                         int *ibest_out, int *ibest2)
{
    double mb;
    int left = 0, right = 0, i;
    if (ibest_out) *ibest_out = ibest;
    if (n < 3) return 0;
    mb = means[ibest];
    for (i = 0; i < ibest; ++i)
        if (means[i] > mb + 0.02) left = 1;
    for (i = ibest + 1; i < n; ++i)
        if (means[i] > mb + 0.02) right = 1;
    return ibest > 0 && ibest < n - 1 && left && right
           && ibest2 && abs(ibest - *ibest2) <= 2;
}

/* ---------- suite: tabu ---------- */

static double run_tabu_mean(const mlab_tsp_inst *inst, const int *tour0,
                            int tenure, int use_2opt,
                            unsigned seed0, int n_runs)
{
    double *vals = (double *)malloc((size_t)n_runs * sizeof(double));
    int i;
    double m;
    if (!vals) return 1e9;
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_tabu_config cfg;
        mlab_tabu_result r;
        mlab_rng_seed_kind(&rng, seed0 + (unsigned)i * 41u, MLAB_RNG_SPLITMIX64);
        cfg.tenure = tenure;
        cfg.max_iter = 8000;
        cfg.max_fevals = BUDGET;
        cfg.use_2opt = use_2opt;
        memset(&r, 0, sizeof r);
        if (!mlab_tabu_tsp(&rng, inst, &cfg, tour0, &r)) {
            vals[i] = 1e9;
            continue;
        }
        vals[i] = r.best_len;
        mlab_tabu_result_free(&r);
    }
    m = mlab_mean(vals, n_runs);
    free(vals);
    return m;
}

static int t_tabu_tenure_u_shape(void)
{
    const int tens[] = {0, 1, 2, 4, 7, 12, 20, 35};
    const int nt = 8;
    const int n_runs = 30;
    double means[8], means2[8];
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, ib = 0, ib2 = 0, ok;
    char detail[200];
    FILE *fp;

    if (!make_cluster_inst(&inst, tour0, 44101)) {
        test_record(0, "tabu", "tenure_u_shape_reproducible", "inst fail");
        return 0;
    }
    for (i = 0; i < nt; ++i) {
        means[i] = run_tabu_mean(&inst, tour0, tens[i], 0 /* swap，任期更敏感 */, 51001, n_runs);
        means2[i] = run_tabu_mean(&inst, tour0, tens[i], 0, 62001, n_runs);
        if (means[i] < means[ib]) ib = i;
        if (means2[i] < means2[ib2]) ib2 = i;
    }
    ok = u_shape_check(means, nt, ib, &ib, &ib2);
    /* 两端相对最优点应明显更差 */
    if (means[ib] > 0 &&
        (means[0] < means[ib] + 0.02 || means[nt - 1] < means[ib] + 0.02))
        ok = 0;
    snprintf(detail, sizeof detail,
             "best_tenure=%d mean=%.4f repro=%d | t0=%.4f t35=%.4f",
             tens[ib], means[ib], tens[ib2], means[0], means[nt - 1]);
    fp = fopen("results/tabu_tenure_scan.csv", "w");
    if (fp) {
        fprintf(fp, "tenure,mean_runA,mean_runB\n");
        for (i = 0; i < nt; ++i)
            fprintf(fp, "%d,%.6f,%.6f\n", tens[i], means[i], means2[i]);
        fclose(fp);
    }
    test_record(ok, "tabu", "tenure_u_shape_reproducible", detail);
    mlab_tsp_free(&inst);
    return ok;
}

/* ---------- suite: tabu — fallback 与长期记忆 ---------- */

/*
 * 全禁忌 fallback（修后语义）：大任期制造全禁忌局面，
 * 随机逃逸必须记入 n_fallback、更新 f_best 并写入禁忌表。
 */
static int t_tabu_fallback_tracks_best(void)
{
    const int n = 12;
    mlab_tsp_inst inst;
    mlab_rng rng;
    mlab_tabu_config cfg;
    mlab_tabu_result r;
    int tour0[12];
    double len0;
    int ok;
    char detail[144];

    memset(&inst, 0, sizeof inst);
    mlab_rng_seed_kind(&rng, 44501, MLAB_RNG_SPLITMIX64);
    if (!mlab_tsp_random(&rng, &inst, n, 10.0)) {
        test_record(0, "tabu", "fallback_tracks_best", "inst fail");
        return 0;
    }
    mlab_tsp_shuffle_tour(&rng, n, tour0);
    len0 = mlab_tsp_length(&inst, tour0);

    cfg.tenure = 80;      /* n=12 swap 邻域 66 个移动对，任期>66 才可能全禁忌 */
    cfg.max_iter = 3000;
    cfg.max_fevals = 30000;
    cfg.use_2opt = 0;
    cfg.freq_lambda = 0.0;
    memset(&r, 0, sizeof r);
    if (!mlab_tabu_tsp(&rng, &inst, &cfg, tour0, &r)) {
        mlab_tsp_free(&inst);
        test_record(0, "tabu", "fallback_tracks_best", "run fail");
        return 0;
    }
    ok = r.n_fallback > 0 && r.best_len < len0
         && r.n_fevals <= 30000 + n * n;
    snprintf(detail, sizeof detail,
             "fallback=%d overrides=%d len0=%.4f best=%.4f fe=%d",
             r.n_fallback, r.n_tabu_overrides, len0, r.best_len, r.n_fevals);
    test_record(ok, "tabu", "fallback_tracks_best", detail);
    mlab_tabu_result_free(&r);
    mlab_tsp_free(&inst);
    return ok;
}

/*
 * TS 长期记忆（频次惩罚）：同预算配对比较 λ=0 vs λ>0。
 * 频次机制应实际生效（n_freq_picks>0）且不劣于关闭状态。
 */
static int t_tabu_freq_memory(void)
{
    const int n_runs = 30;
    double *l_off, *l_on;
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, ok, freq_picks = 0;
    double m_off, m_on;
    char detail[160];
    FILE *fp;

    l_off = (double *)malloc((size_t)n_runs * sizeof(double));
    l_on = (double *)malloc((size_t)n_runs * sizeof(double));
    if (!l_off || !l_on) {
        free(l_off); free(l_on);
        test_record(0, "tabu", "freq_memory_diversifies", "oom");
        return 0;
    }
    if (!make_cluster_inst(&inst, tour0, 44601)) {
        free(l_off); free(l_on);
        test_record(0, "tabu", "freq_memory_diversifies", "inst fail");
        return 0;
    }
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_tabu_config cfg;
        mlab_tabu_result r;
        memset(&cfg, 0, sizeof cfg);
        cfg.tenure = 8;
        cfg.max_iter = 4000;
        cfg.max_fevals = BUDGET;
        cfg.use_2opt = 1;
        mlab_rng_seed_kind(&rng, 80000u + (unsigned)i * 43u,
                           MLAB_RNG_SPLITMIX64);
        memset(&r, 0, sizeof r);
        if (mlab_tabu_tsp(&rng, &inst, &cfg, tour0, &r)) {
            l_off[i] = r.best_len;
            mlab_tabu_result_free(&r);
        } else l_off[i] = 1e9;
        cfg.freq_lambda = 0.05;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 80000u + (unsigned)i * 43u,
                           MLAB_RNG_SPLITMIX64);
        if (mlab_tabu_tsp(&rng, &inst, &cfg, tour0, &r)) {
            l_on[i] = r.best_len;
            freq_picks += r.n_freq_picks;
            mlab_tabu_result_free(&r);
        } else l_on[i] = 1e9;
    }
    m_off = mlab_mean(l_off, n_runs);
    m_on = mlab_mean(l_on, n_runs);
    ok = freq_picks > 0 && m_on < m_off + 0.01;
    snprintf(detail, sizeof detail,
             "lambda=0: %.4f | lambda=0.05: %.4f (freq_picks=%d)",
             m_off, m_on, freq_picks);
    fp = fopen("results/tabu_freq_memory.csv", "w");
    if (fp) {
        fprintf(fp, "run,len_lambda0,len_lambda002\n");
        for (i = 0; i < n_runs; ++i)
            fprintf(fp, "%d,%.6f,%.6f\n", i, l_off[i], l_on[i]);
        fclose(fp);
    }
    test_record(ok, "tabu", "freq_memory_diversifies", detail);
    mlab_tsp_free(&inst);
    free(l_off); free(l_on);
    return ok;
}

/* ---------- suite: vns ---------- */

static void run_vns_set(const mlab_tsp_inst *inst, const int *tour0,
                        unsigned mask, int vnd,
                        unsigned seed0, int n_runs, double *out_lens)
{
    int i;
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_vns_config cfg;
        mlab_vns_result r;
        mlab_rng_seed_kind(&rng, seed0 + (unsigned)i * 37u, MLAB_RNG_SPLITMIX64);
        cfg.nbhd_mask = mask;
        cfg.max_fevals = BUDGET;
        cfg.max_outer = 4000;
        cfg.ls_pass_limit = BUDGET;
        cfg.vnd = vnd;
        cfg.kmax = 0; /* 默认 5 */
        memset(&r, 0, sizeof r);
        if (!mlab_vns_tsp(&rng, inst, &cfg, tour0, &r)) {
            out_lens[i] = 1e9;
            continue;
        }
        out_lens[i] = r.best_len;
        mlab_vns_result_free(&r);
    }
}

static int t_vns_multi_vs_single(void)
{
    const int n_runs = 40;
    const int n_cities = 24;
    double *l_swap, *l_2opt, *l_or, *l_multi;
    mlab_tsp_inst inst;
    int tour0[24];
    double ms, m2, mo, mm, p_s, p_2, p_o;
    int ok;
    char detail[220];
    FILE *fp;
    int i;

    l_swap = (double *)malloc((size_t)n_runs * sizeof(double));
    l_2opt = (double *)malloc((size_t)n_runs * sizeof(double));
    l_or = (double *)malloc((size_t)n_runs * sizeof(double));
    l_multi = (double *)malloc((size_t)n_runs * sizeof(double));
    if (!l_swap || !l_2opt || !l_or || !l_multi) {
        free(l_swap); free(l_2opt); free(l_or); free(l_multi);
        test_record(0, "vns", "multi_beats_single_neighborhoods", "oom");
        return 0;
    }
    /* 均匀随机欧氏实例（n=24）：2-opt 局部最优平庸，or-opt/swap 可互补 */
    {
        mlab_rng rng;
        memset(&inst, 0, sizeof inst);
        mlab_rng_seed_kind(&rng, 44201, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            free(l_swap); free(l_2opt); free(l_or); free(l_multi);
            test_record(0, "vns", "multi_beats_single_neighborhoods", "inst fail");
            return 0;
        }
        mlab_tsp_shuffle_tour(&rng, n_cities, tour0);
    }
    /* 去混杂：四个臂全部 vnd=1，唯一变量是邻域集合 */
    run_vns_set(&inst, tour0, MLAB_NB_SWAP, 1, 71001, n_runs, l_swap);
    run_vns_set(&inst, tour0, MLAB_NB_2OPT, 1, 71001, n_runs, l_2opt);
    run_vns_set(&inst, tour0, MLAB_NB_OROPT, 1, 71001, n_runs, l_or);
    run_vns_set(&inst, tour0, MLAB_NB_ALL, 1, 71001, n_runs, l_multi);
    (void)n_cities;
    ms = mlab_mean(l_swap, n_runs);
    m2 = mlab_mean(l_2opt, n_runs);
    mo = mlab_mean(l_or, n_runs);
    mm = mlab_mean(l_multi, n_runs);
    p_s = paired_p_two_sided(l_swap, l_multi, n_runs);
    p_2 = paired_p_two_sided(l_2opt, l_multi, n_runs);
    p_o = paired_p_two_sided(l_or, l_multi, n_runs);
    ok = mm < ms && mm < m2 && mm < mo
         && p_s < 0.01 && p_2 < 0.01 && p_o < 0.01;
    snprintf(detail, sizeof detail,
             "vnd=1 all arms: swap=%.4f 2opt=%.4f oropt=%.4f multi=%.4f | p_s=%.2e p_2=%.2e p_o=%.2e",
             ms, m2, mo, mm, p_s, p_2, p_o);
    test_record(ok, "vns", "multi_beats_single_neighborhoods", detail);
    fp = fopen("results/vns_nbhd_compare.csv", "w");
    if (fp) {
        fprintf(fp, "pair,len_swap,len_2opt,len_oropt,len_multi\n");
        for (i = 0; i < n_runs; ++i)
            fprintf(fp, "%d,%.6f,%.6f,%.6f,%.6f\n",
                    i, l_swap[i], l_2opt[i], l_or[i], l_multi[i]);
        fclose(fp);
    }
    mlab_tsp_free(&inst);
    free(l_swap); free(l_2opt); free(l_or); free(l_multi);
    return ok;
}

/* ---------- suite: vns — best vs first improvement ---------- */

/* 穷举验证 tour 是否为 mask 邻域下的局部最优（判定“收敛到局部最优”） */
static int verify_local_opt(const mlab_tsp_inst *inst, const int *tour,
                            unsigned mask)
{
    int n = inst->n;
    double base = mlab_tsp_length(inst, tour);
    int *cand = (int *)malloc((size_t)n * sizeof(int));
    int i, j, ok = 1;
    if (!cand) return 0;
    for (i = 0; i < n && ok; ++i) {
        for (j = 0; j < n; ++j) {
            double flen;
            if ((mask & MLAB_NB_SWAP) && j > i) {
                memcpy(cand, tour, (size_t)n * sizeof(int));
                mlab_tsp_apply_swap(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                if (flen < base - 1e-9) { ok = 0; break; }
            }
            if ((mask & MLAB_NB_2OPT) && j > i) {
                memcpy(cand, tour, (size_t)n * sizeof(int));
                mlab_tsp_apply_2opt(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                if (flen < base - 1e-9) { ok = 0; break; }
            }
            if ((mask & MLAB_NB_OROPT) && i != j) {
                memcpy(cand, tour, (size_t)n * sizeof(int));
                mlab_tsp_apply_oropt(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                if (flen < base - 1e-9) { ok = 0; break; }
            }
        }
    }
    free(cand);
    return ok;
}

/*
 * best vs first improvement（路线图 :435）：
 * 同起点同预算跑到局部最优，两者都应停在（穷举验证的）局部最优；
 * 记录质量与评估成本，best 通常质量略优、first 评估更省。
 */
static int t_vns_best_vs_first(void)
{
    const int n_runs = 30;
    double *l_first, *l_best, *e_first, *e_best;
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, ok, opt_first = 1, opt_best = 1;
    double mf, mb, ef, eb;
    char detail[200];
    FILE *fp;

    l_first = (double *)malloc((size_t)n_runs * sizeof(double));
    l_best = (double *)malloc((size_t)n_runs * sizeof(double));
    e_first = (double *)malloc((size_t)n_runs * sizeof(double));
    e_best = (double *)malloc((size_t)n_runs * sizeof(double));
    if (!l_first || !l_best || !e_first || !e_best) {
        free(l_first); free(l_best); free(e_first); free(e_best);
        test_record(0, "vns", "best_vs_first_improvement", "oom");
        return 0;
    }
    if (!make_cluster_inst(&inst, tour0, 44301)) {
        free(l_first); free(l_best); free(e_first); free(e_best);
        test_record(0, "vns", "best_vs_first_improvement", "inst fail");
        return 0;
    }
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        int *work = (int *)malloc(N_CITIES * sizeof(int));
        double len;
        int fe;
        if (!work) break;
        mlab_rng_seed_kind(&rng, 73000u + (unsigned)i * 41u,
                           MLAB_RNG_SPLITMIX64);
        memcpy(work, tour0, sizeof tour0);
        /* 同一随机扰动序列作为起点，保证两臂同起点 */
        mlab_tsp_shuffle_tour(&rng, N_CITIES, work);
        len = mlab_tsp_length(&inst, work);
        fe = 1;
        while (mlab_tsp_first_improve(&inst, work, &len, MLAB_NB_ALL,
                                      20000, &fe))
            ;
        l_first[i] = len;
        e_first[i] = (double)fe;
        if (!verify_local_opt(&inst, work, MLAB_NB_ALL)) opt_first = 0;
        mlab_rng_seed_kind(&rng, 73000u + (unsigned)i * 41u,
                           MLAB_RNG_SPLITMIX64);
        memcpy(work, tour0, sizeof tour0);
        mlab_tsp_shuffle_tour(&rng, N_CITIES, work);
        len = mlab_tsp_length(&inst, work);
        fe = 1;
        while (mlab_tsp_best_improve(&inst, work, &len, MLAB_NB_ALL,
                                     20000, &fe))
            ;
        l_best[i] = len;
        e_best[i] = (double)fe;
        if (!verify_local_opt(&inst, work, MLAB_NB_ALL)) opt_best = 0;
        free(work);
    }
    mf = mlab_mean(l_first, n_runs);
    mb = mlab_mean(l_best, n_runs);
    ef = mlab_mean(e_first, n_runs);
    eb = mlab_mean(e_best, n_runs);
    ok = opt_first && opt_best && mb <= mf + 0.01;
    snprintf(detail, sizeof detail,
             "first: len=%.4f fe=%.0f | best: len=%.4f fe=%.0f | "
             "local_opt first=%d best=%d",
             mf, ef, mb, eb, opt_first, opt_best);
    fp = fopen("results/vns_best_vs_first.csv", "w");
    if (fp) {
        fprintf(fp, "run,len_first,fe_first,len_best,fe_best\n");
        for (i = 0; i < n_runs; ++i)
            fprintf(fp, "%d,%.6f,%.0f,%.6f,%.0f\n",
                    i, l_first[i], e_first[i], l_best[i], e_best[i]);
        fclose(fp);
    }
    test_record(ok, "vns", "best_vs_first_improvement", detail);
    mlab_tsp_free(&inst);
    free(l_first); free(l_best); free(e_first); free(e_best);
    return ok;
}

/* ---------- suite: ils ---------- */

/*
 * 每次运行随机初始回路 → 完整局部搜索到局部最优 L0 →
 * 固定预算 ILS。s=0 对照 = 停在 L0；过弱难逃离；过强浪费预算。
 */
static double run_ils_mean_n(int n_cities, int strength, unsigned mask,
                             int ls_pass, int max_outer, int max_fe,
                             unsigned seed0, int n_runs)
{
    double *vals = (double *)malloc((size_t)n_runs * sizeof(double));
    int i;
    double m;
    if (!vals) return 1e9;
    for (i = 0; i < n_runs; ++i) {
        mlab_tsp_inst inst;
        mlab_rng rng;
        mlab_ils_config cfg;
        mlab_ils_result r;
        int *tour0;
        memset(&inst, 0, sizeof inst);
        mlab_rng_seed_kind(&rng, seed0 + (unsigned)i * 53u, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random_matrix(&rng, &inst, n_cities, 1.0, 8.0)) {
            vals[i] = 1e9;
            continue;
        }
        tour0 = (int *)malloc((size_t)n_cities * sizeof(int));
        if (!tour0) {
            mlab_tsp_free(&inst);
            vals[i] = 1e9;
            continue;
        }
        mlab_tsp_shuffle_tour(&rng, n_cities, tour0);
        cfg.pert_strength = strength;
        cfg.nbhd_mask = mask;
        cfg.max_fevals = max_fe;
        cfg.max_outer = max_outer;
        cfg.ls_pass_limit = ls_pass;
        cfg.accept_worse = 0;
        memset(&r, 0, sizeof r);
        if (!mlab_ils_tsp(&rng, &inst, &cfg, tour0, &r)) {
            vals[i] = 1e9;
        } else {
            vals[i] = r.best_len;
            mlab_ils_result_free(&r);
        }
        free(tour0);
        mlab_tsp_free(&inst);
    }
    m = mlab_mean(vals, n_runs);
    free(vals);
    return m;
}

static int t_ils_perturbation_u_shape(void)
{
    /* s=0 对照（无扰动）→ 中等最优 → 过强劣化 */
    const int strengths[] = {0, 1, 2, 3, 5, 8, 12, 20, 40};
    const int ns = 9;
    const int n_runs = 25;
    const int n_city = 24;
    double means[9], means2[9];
    int i, ib = 0, ib2 = 0, ok;
    char detail[220];
    FILE *fp;

    for (i = 0; i < ns; ++i) {
        /* 总预算须大于初始 LS，否则主循环不启动 */
        means[i] = run_ils_mean_n(n_city, strengths[i], MLAB_NB_ALL,
                                  4000, 40, 25000,
                                  81001, n_runs);
        means2[i] = run_ils_mean_n(n_city, strengths[i], MLAB_NB_ALL,
                                   4000, 40, 25000,
                                   92001, n_runs);
        if (means[i] < means[ib]) ib = i;
        if (means2[i] < means2[ib2]) ib2 = i;
    }
    /*
     * U 形：最优点不在扫描端点；左右两侧均存在更差的点；
     * 两次独立种子最优强度落在同一邻域（|Δ|≤2）。
     */
    ok = u_shape_check(means, ns, ib, &ib, &ib2);
    /* 两端（s=0 与 s=40）不应优于最优点 */
    if (means[0] < means[ib] - 1e-9 || means[ns - 1] < means[ib] - 1e-9)
        ok = 0;
    snprintf(detail, sizeof detail,
             "best_s=%d mean=%.4f repro_s=%d | s0=%.4f s40=%.4f",
             strengths[ib], means[ib], strengths[ib2], means[0], means[ns - 1]);
    fp = fopen("results/ils_pert_scan.csv", "w");
    if (fp) {
        fprintf(fp, "strength,mean_runA,mean_runB\n");
        for (i = 0; i < ns; ++i)
            fprintf(fp, "%d,%.6f,%.6f\n", strengths[i], means[i], means2[i]);
        fclose(fp);
    }
    test_record(ok, "ils", "perturbation_u_shape_reproducible", detail);
    return ok;
}

/* ---------- suite: compare ---------- */

static int t_four_mechanism_table(void)
{
    const int n_runs = 100;
    double *l_sa, *l_ts, *l_vns, *l_ils;
    double *f_sa, *f_ts, *f_vns, *f_ils;
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, ok;
    char detail[220];
    FILE *fp;

    l_sa = (double *)malloc((size_t)n_runs * sizeof(double));
    l_ts = (double *)malloc((size_t)n_runs * sizeof(double));
    l_vns = (double *)malloc((size_t)n_runs * sizeof(double));
    l_ils = (double *)malloc((size_t)n_runs * sizeof(double));
    f_sa = (double *)malloc((size_t)n_runs * sizeof(double));
    f_ts = (double *)malloc((size_t)n_runs * sizeof(double));
    f_vns = (double *)malloc((size_t)n_runs * sizeof(double));
    f_ils = (double *)malloc((size_t)n_runs * sizeof(double));
    if (!l_sa || !l_ts || !l_vns || !l_ils || !f_sa || !f_ts || !f_vns || !f_ils) {
        free(l_sa); free(l_ts); free(l_vns); free(l_ils);
        free(f_sa); free(f_ts); free(f_vns); free(f_ils);
        test_record(0, "compare", "four_mechanisms_same_budget", "oom");
        return 0;
    }
    if (!make_cluster_inst(&inst, tour0, 44401)) {
        free(l_sa); free(l_ts); free(l_vns); free(l_ils);
        free(f_sa); free(f_ts); free(f_vns); free(f_ils);
        test_record(0, "compare", "four_mechanisms_same_budget", "inst fail");
        return 0;
    }

    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 90000u + (unsigned)i * 29u;
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        {
            mlab_tsp_sa_config cfg;
            mlab_tsp_sa_result r;
            cfg.T0 = 2.5;
            cfg.alpha = 0.98;
            cfg.T_min = 1e-3;
            cfg.inner = 25;
            cfg.max_outer = BUDGET / 25 + 8;
            cfg.nbhd = MLAB_TSP_NBHD_2OPT;
            memset(&r, 0, sizeof r);
            if (mlab_sa_tsp(&rng, &inst, &cfg, tour0, &r)) {
                l_sa[i] = r.best_len;
                f_sa[i] = r.n_fevals;
            } else {
                l_sa[i] = 1e9;
                f_sa[i] = 0;
            }
            mlab_tsp_sa_result_free(&r);
        }
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        {
            mlab_tabu_config cfg;
            mlab_tabu_result r;
            cfg.tenure = 8;
            cfg.max_iter = 8000;
            cfg.max_fevals = BUDGET;
            cfg.use_2opt = 1;
            memset(&r, 0, sizeof r);
            if (mlab_tabu_tsp(&rng, &inst, &cfg, tour0, &r)) {
                l_ts[i] = r.best_len;
                f_ts[i] = r.n_fevals;
            } else {
                l_ts[i] = 1e9;
                f_ts[i] = 0;
            }
            mlab_tabu_result_free(&r);
        }
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        {
            mlab_vns_config cfg;
            mlab_vns_result r;
            cfg.nbhd_mask = MLAB_NB_ALL;
            cfg.max_fevals = BUDGET;
            cfg.max_outer = 4000;
            cfg.ls_pass_limit = BUDGET;
            cfg.vnd = 1;
            memset(&r, 0, sizeof r);
            if (mlab_vns_tsp(&rng, &inst, &cfg, tour0, &r)) {
                l_vns[i] = r.best_len;
                f_vns[i] = r.n_fevals;
            } else {
                l_vns[i] = 1e9;
                f_vns[i] = 0;
            }
            mlab_vns_result_free(&r);
        }
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        {
            mlab_ils_config cfg;
            mlab_ils_result r;
            cfg.pert_strength = 7;
            cfg.nbhd_mask = MLAB_NB_ALL;
            cfg.max_fevals = BUDGET;
            cfg.max_outer = 4000;
            cfg.ls_pass_limit = BUDGET;
            cfg.accept_worse = 0;
            memset(&r, 0, sizeof r);
            if (mlab_ils_tsp(&rng, &inst, &cfg, tour0, &r)) {
                l_ils[i] = r.best_len;
                f_ils[i] = r.n_fevals;
            } else {
                l_ils[i] = 1e9;
                f_ils[i] = 0;
            }
            mlab_ils_result_free(&r);
        }
    }

    {
        double ms = mlab_mean(l_sa, n_runs);
        double mt = mlab_mean(l_ts, n_runs);
        double mv = mlab_mean(l_vns, n_runs);
        double mi = mlab_mean(l_ils, n_runs);
        double fs = mlab_mean(f_sa, n_runs);
        double ft = mlab_mean(f_ts, n_runs);
        double fv = mlab_mean(f_vns, n_runs);
        double fi = mlab_mean(f_ils, n_runs);
        /* 同预算判据：各机制平均评估数须在预算 ±12% 内 */
        int budget_ok =
            fabs(fs - BUDGET) < 0.12 * BUDGET &&
            fabs(ft - BUDGET) < 0.12 * BUDGET &&
            fabs(fv - BUDGET) < 0.12 * BUDGET &&
            fabs(fi - BUDGET) < 0.12 * BUDGET;
        ok = ms < 1e8 && mt < 1e8 && mv < 1e8 && mi < 1e8
             && fs > 200 && ft > 200 && fv > 200 && fi > 200
             && budget_ok;
        snprintf(detail, sizeof detail,
                 "SA=%.3f/%.0f TS=%.3f/%.0f VNS=%.3f/%.0f ILS=%.3f/%.0f (len/fe n=%d)",
                 ms, fs, mt, ft, mv, fv, mi, fi, n_runs);
        fp = fopen("results/four_mechanisms.csv", "w");
        if (fp) {
            fprintf(fp, "algo,mean_len,sd_len,mean_fevals,min_len\n");
            fprintf(fp, "SA,%.6f,%.6f,%.2f,%.6f\n", ms, mlab_std(l_sa, n_runs), fs, vec_min(l_sa, n_runs));
            fprintf(fp, "TS,%.6f,%.6f,%.2f,%.6f\n", mt, mlab_std(l_ts, n_runs), ft, vec_min(l_ts, n_runs));
            fprintf(fp, "VNS,%.6f,%.6f,%.2f,%.6f\n", mv, mlab_std(l_vns, n_runs), fv, vec_min(l_vns, n_runs));
            fprintf(fp, "ILS,%.6f,%.6f,%.2f,%.6f\n", mi, mlab_std(l_ils, n_runs), fi, vec_min(l_ils, n_runs));
            fprintf(fp, "pair_i,SA,TS,VNS,ILS\n");
            for (i = 0; i < n_runs; ++i)
                fprintf(fp, "%d,%.6f,%.6f,%.6f,%.6f\n", i, l_sa[i], l_ts[i], l_vns[i], l_ils[i]);
            fclose(fp);
        }
    }
    test_record(ok, "compare", "four_mechanisms_same_budget", detail);
    mlab_tsp_free(&inst);
    free(l_sa); free(l_ts); free(l_vns); free(l_ils);
    free(f_sa); free(f_ts); free(f_vns); free(f_ils);
    return ok;
}

/* ---------- suite: ils — 对比随机多重启局部搜索 ---------- */

/*
 * 同预算 ILS vs 随机多重启局部搜索（回溯记忆 vs 无记忆）：
 * ILS 从上一局部最优扰动出发，应优于“随机起点+LS 到局部最优”重复。
 */
static int t_ils_beats_random_restarts(void)
{
    const int n_city = 20;
    const int n_inst = 60;
    double *l_ils, *l_rs;
    int i, ok;
    double mi, mr, p;
    char detail[176];
    FILE *fp;

    l_ils = (double *)malloc((size_t)n_inst * sizeof(double));
    l_rs = (double *)malloc((size_t)n_inst * sizeof(double));
    if (!l_ils || !l_rs) {
        free(l_ils); free(l_rs);
        test_record(0, "ils", "beats_random_restarts", "oom");
        return 0;
    }
    for (i = 0; i < n_inst; ++i) {
        mlab_tsp_inst inst;
        mlab_rng rng;
        int *tour0;
        mlab_ils_config cfg;
        mlab_ils_result r;
        double best_rs;
        memset(&inst, 0, sizeof inst);
        mlab_rng_seed_kind(&rng, 85000u + (unsigned)i * 61u,
                           MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random_matrix(&rng, &inst, n_city, 1.0, 8.0)) {
            l_ils[i] = l_rs[i] = 1e9;
            continue;
        }
        tour0 = (int *)malloc((size_t)n_city * sizeof(int));
        if (!tour0) {
            mlab_tsp_free(&inst);
            l_ils[i] = l_rs[i] = 1e9;
            continue;
        }
        mlab_tsp_shuffle_tour(&rng, n_city, tour0);
        /* ILS：同预算（放大两臂差异：预算 10000） */
        cfg.pert_strength = 4;
        cfg.nbhd_mask = MLAB_NB_2OPT;
        cfg.max_fevals = 10000;
        cfg.max_outer = 6000;
        cfg.ls_pass_limit = 4000;
        cfg.accept_worse = 0;
        memset(&r, 0, sizeof r);
        if (mlab_ils_tsp(&rng, &inst, &cfg, tour0, &r)) {
            l_ils[i] = r.best_len;
            mlab_ils_result_free(&r);
        } else l_ils[i] = 1e9;
        /* 随机多重启：随机起点 → VND 到局部最优，直至预算耗尽 */
        best_rs = 1e300;
        {
            int fe = 0, guard;
            for (guard = 0; guard < 300 && fe < 10000; ++guard) {
                int *t = (int *)malloc((size_t)n_city * sizeof(int));
                double len;
                if (!t) break;
                mlab_tsp_shuffle_tour(&rng, n_city, t);
                len = mlab_tsp_length(&inst, t);
                ++fe;
                mlab_tsp_vnd(&inst, t, &len, MLAB_NB_2OPT,
                             10000 - fe > 0 ? 10000 - fe : 0, &fe);
                if (len < best_rs) best_rs = len;
                free(t);
            }
        }
        l_rs[i] = best_rs;
        free(tour0);
        mlab_tsp_free(&inst);
    }
    mi = mlab_mean(l_ils, n_inst);
    mr = mlab_mean(l_rs, n_inst);
    p = paired_p_two_sided(l_rs, l_ils, n_inst);
    ok = mi < mr && p < 0.01;
    snprintf(detail, sizeof detail,
             "ils=%.4f restarts=%.4f paired_p=%.2e (need ils<restarts, p<0.01)",
             mi, mr, p);
    {
        FILE *fp2 = fopen("results/ils_vs_restarts.csv", "w");
        if (fp2) {
            int k;
            fprintf(fp2, "inst,len_ils,len_restarts\n");
            for (k = 0; k < n_inst; ++k)
                fprintf(fp2, "%d,%.6f,%.6f\n", k, l_ils[k], l_rs[k]);
            fclose(fp2);
        }
    }
    (void)fp;
    test_record(ok, "ils", "beats_random_restarts", detail);
    free(l_ils); free(l_rs);
    return ok;
}

/* ---------- suite: lns / grasp（路线图注记项落地） ---------- */

/*
 * LNS 毁灭-重建：从 VND 局部最优出发，同预算下
 * “继续 VND”（已无改进空间）应劣于 LNS（跨出邻域族）。
 */
static int t_lns_escapes_local_optimum(void)
{
    const int n_runs = 20;
    double *l_more, *l_lns;
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, ok;
    double mv, ml;
    char detail[176];
    FILE *fp;

    l_more = (double *)malloc((size_t)n_runs * sizeof(double));
    l_lns = (double *)malloc((size_t)n_runs * sizeof(double));
    if (!l_more || !l_lns) {
        free(l_more); free(l_lns);
        test_record(0, "lns", "escapes_local_optimum", "oom");
        return 0;
    }
    if (!make_cluster_inst(&inst, tour0, 44701)) {
        free(l_more); free(l_lns);
        test_record(0, "lns", "escapes_local_optimum", "inst fail");
        return 0;
    }
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        int *work = (int *)malloc(N_CITIES * sizeof(int));
        double len, len0;
        int fe;
        if (!work) break;
        mlab_rng_seed_kind(&rng, 87000u + (unsigned)i * 47u,
                           MLAB_RNG_SPLITMIX64);
        memcpy(work, tour0, sizeof tour0);
        mlab_tsp_shuffle_tour(&rng, N_CITIES, work);
        len = mlab_tsp_length(&inst, work);
        fe = 1;
        mlab_tsp_vnd(&inst, work, &len, MLAB_NB_ALL, BUDGET, &fe);
        len0 = len;
        /* 臂 A：继续 VND 同预算（局部最优 → 无改进） */
        {
            int fe2 = fe;
            double l2 = len;
            mlab_tsp_vnd(&inst, work, &l2, MLAB_NB_ALL, BUDGET, &fe2);
            l_more[i] = l2;
        }
        /* 臂 B：LNS 毁灭 5 城 + 最小代价重插 + VND */
        {
            mlab_lns_config cfg;
            mlab_lns_result r;
            memset(&cfg, 0, sizeof cfg);
            cfg.destroy_count = 5;
            cfg.nbhd_mask = MLAB_NB_ALL;
            cfg.max_fevals = BUDGET;
            cfg.max_outer = 4000;
            cfg.ls_pass_limit = 2000;
            memset(&r, 0, sizeof r);
            if (mlab_lns_tsp(&rng, &inst, &cfg, work, &r)) {
                l_lns[i] = r.best_len;
                mlab_lns_result_free(&r);
            } else l_lns[i] = 1e9;
        }
        (void)len0;
        free(work);
    }
    mv = mlab_mean(l_more, n_runs);
    ml = mlab_mean(l_lns, n_runs);
    ok = ml < mv;
    snprintf(detail, sizeof detail,
             "+vnd=%.4f +lns5=%.4f (need lns<vnd，同预算 BUDGET=%d)",
             mv, ml, BUDGET);
    fp = fopen("results/lns_vs_vnd.csv", "w");
    if (fp) {
        fprintf(fp, "run,len_vnd,len_lns\n");
        for (i = 0; i < n_runs; ++i)
            fprintf(fp, "%d,%.6f,%.6f\n", i, l_more[i], l_lns[i]);
        fclose(fp);
    }
    test_record(ok, "lns", "escapes_local_optimum", detail);
    mlab_tsp_free(&inst);
    free(l_more); free(l_lns);
    return ok;
}

/*
 * GRASP RCL 松紧：alpha=0（纯贪心 NN，确定性）与 alpha=1（纯随机）
 * 两端都应劣于中间值（贪心随机自适应）；同预算三臂对照。
 */
static int t_grasp_rcl_balance(void)
{
    const int n_runs = 20;
    const double alphas[3] = {0.0, 0.5, 1.0};
    double means[3];
    mlab_tsp_inst inst;
    int tour0[N_CITIES];
    int i, a, ok, ib = 0;
    char detail[176];
    FILE *fp;

    if (!make_cluster_inst(&inst, tour0, 44801)) {
        test_record(0, "grasp", "rcl_balance_interior", "inst fail");
        return 0;
    }
    for (a = 0; a < 3; ++a) {
        double *vals = (double *)malloc((size_t)n_runs * sizeof(double));
        for (i = 0; i < n_runs; ++i) {
            mlab_rng rng;
            mlab_grasp_config cfg;
            mlab_grasp_result r;
            memset(&cfg, 0, sizeof cfg);
            cfg.rcl_alpha = alphas[a];
            cfg.n_iter = 40;
            cfg.nbhd_mask = MLAB_NB_2OPT;
            cfg.max_fevals = BUDGET;
            cfg.ls_pass_limit = 2000;
            mlab_rng_seed_kind(&rng, 88000u + (unsigned)a * 1000u
                                          + (unsigned)i * 53u,
                               MLAB_RNG_SPLITMIX64);
            memset(&r, 0, sizeof r);
            if (mlab_grasp_tsp(&rng, &inst, &cfg, &r)) {
                vals[i] = r.best_len;
                mlab_grasp_result_free(&r);
            } else vals[i] = 1e9;
        }
        means[a] = mlab_mean(vals, n_runs);
        free(vals);
        if (means[a] < means[ib]) ib = a;
    }
    ok = ib == 1; /* 最优在中间（贪心随机自适应） */
    snprintf(detail, sizeof detail,
             "alpha0=%.4f alpha0.5=%.4f alpha1=%.4f (need best=alpha0.5)",
             means[0], means[1], means[2]);
    fp = fopen("results/grasp_rcl_scan.csv", "w");
    if (fp) {
        fprintf(fp, "alpha,mean_len\n");
        for (a = 0; a < 3; ++a)
            fprintf(fp, "%.2f,%.6f\n", alphas[a], means[a]);
        fclose(fp);
    }
    test_record(ok, "grasp", "rcl_balance_interior", detail);
    mlab_tsp_free(&inst);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"tabu", "tenure_u_shape_reproducible",
         "禁忌表长度 U 形且最优区间可复现",
         t_tabu_tenure_u_shape},
        {"tabu", "fallback_tracks_best",
         "全禁忌随机逃逸更新 f_best/禁忌表（修后语义）",
         t_tabu_fallback_tracks_best},
        {"tabu", "freq_memory_diversifies",
         "长期记忆频次惩罚生效且不劣化（λ=0 vs 0.05）",
         t_tabu_freq_memory},
        {"vns", "multi_beats_single_neighborhoods",
         "多邻域 VNS 优于任一单邻域（vnd=1 去混杂，p<0.01）",
         t_vns_multi_vs_single},
        {"vns", "best_vs_first_improvement",
         "best vs first improvement：都停在局部最优，成本对照",
         t_vns_best_vs_first},
        {"ils", "perturbation_u_shape_reproducible",
         "ILS 扰动强度 U 形且区间可复现",
         t_ils_perturbation_u_shape},
        {"ils", "beats_random_restarts",
         "同预算 ILS 优于随机多重启局部搜索（p<0.01）",
         t_ils_beats_random_restarts},
        {"lns", "escapes_local_optimum",
         "LNS 毁灭-重建超越局部最优（同预算 vs 继续 VND）",
         t_lns_escapes_local_optimum},
        {"grasp", "rcl_balance_interior",
         "GRASP RCL 松紧：最优在纯贪心与纯随机之间",
         t_grasp_rcl_balance},
        {"compare", "four_mechanisms_same_budget",
         "SA/TS/VNS/ILS 同预算（±12%）≥100 次对账表",
         t_four_mechanism_table},
    };
    test_ensure_results_dir();
    return test_run_main("C4-ts-vns-ils", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
