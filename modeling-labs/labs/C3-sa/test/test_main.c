/*
 * C3: 模拟退火 — Rastrigin 调参/热图/接受率 + TSP swap vs 2-opt
 * 判据:
 *  - 调参后 1000 次 2D Rastrigin 全局最优命中率 ≥ 80%
 *  - 参数网格热图完整；同参数复跑命中率波动 < ±5pp
 *  - 2-opt 邻域最终回路显著优于 swap（配对检验 p < 0.01）
 */
#include "harness.h"
#include "lab.h"
#include "sa.h"
#include "tsp.h"
#include "bench_sa.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tune 试搜：α=0.99, inner=40, step=0.20, T0=1 → 命中率≈1.0 */
typedef struct {
    double alpha;
    int inner;
    double step;
    double T0;
} sa_params;

static const sa_params k_tuned = {0.99, 40, 0.20, 1.0};

static int rastrigin_hit(double f) { return f < 1e-6; }

static double run_rastrigin_rate(const sa_params *p, int calib,
                                 unsigned seed0, int n_runs, int *fevals_out)
{
    double lb[2], ub[2];
    mlab_sa_problem prob;
    mlab_sa_config cfg;
    int i, h = 0;
    long fe = 0;

    mlab_rastrigin_bounds(2, lb, ub);
    prob.dim = 2;
    prob.f = mlab_rastrigin;
    prob.ctx = NULL;
    prob.lb = lb;
    prob.ub = ub;
    memset(&cfg, 0, sizeof cfg);
    cfg.T0 = p->T0;
    cfg.alpha = p->alpha;
    cfg.inner = p->inner;
    cfg.T_min = 1e-4;
    cfg.step_scale = p->step;
    cfg.calibrate_T0 = calib;
    cfg.target_accept = 0.8;
    cfg.max_outer = 0;
    cfg.step_fixed = 0;

    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_sa_result r;
        double x0[2];
        mlab_rng_seed_kind(&rng, seed0 + (unsigned)i * 97u + 13u,
                           MLAB_RNG_SPLITMIX64);
        x0[0] = lb[0] + mlab_rng_uniform(&rng) * (ub[0] - lb[0]);
        x0[1] = lb[1] + mlab_rng_uniform(&rng) * (ub[1] - lb[1]);
        if (!mlab_sa_continuous(&rng, &prob, &cfg, x0, &r)) continue;
        if (rastrigin_hit(r.f_best)) ++h;
        fe += r.n_fevals;
        mlab_sa_result_free(&r);
    }
    if (fevals_out) *fevals_out = (int)(n_runs > 0 ? fe / n_runs : 0);
    return (double)h / (double)n_runs;
}

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

/* ---------- suite: sa ---------- */

static int t_sa_rastrigin_tuned_1000(void)
{
    int avg_fe = 0;
    double rate = run_rastrigin_rate(&k_tuned, 0, 73001, 1000, &avg_fe);
    int ok = rate >= 0.80;
    char d[160];
    FILE *fp;

    snprintf(d, sizeof d,
             "hit_rate=%.3f (need>=0.80) alpha=%.2f inner=%d step=%.2f T0=%.2f avg_fe=%d",
             rate, k_tuned.alpha, k_tuned.inner, k_tuned.step, k_tuned.T0, avg_fe);
    test_record(ok, "sa", "rastrigin_tuned_1000_runs", d);
    fp = fopen("results/sa_hit_rate.csv", "w");
    if (fp) {
        fprintf(fp, "test,alpha,inner,step,T0,n_runs,hit_rate,avg_fevals\n");
        fprintf(fp, "tuned,%.3f,%d,%.3f,%.3f,1000,%.4f,%d\n",
                k_tuned.alpha, k_tuned.inner, k_tuned.step, k_tuned.T0, rate, avg_fe);
        fclose(fp);
    }
    return ok;
}

static int t_sa_param_scan_heatmap(void)
{
    const double alphas[] = {0.90, 0.95, 0.98, 0.99};
    const int inners[] = {10, 20, 40};
    const double T0s[] = {0.5, 1.0, 5.0, 20.0};
    const int na = 4, ni = 3, nt = 4;
    const int n_runs = 60;
    const double step = 0.20;
    double *rates;
    unsigned char *img;
    int ia, ii, it, cells = na * ni * nt, filled = 0;
    int ok_grid, ok_repro = 1;
    FILE *fp, *pgm;
    char detail[192];

    rates = (double *)malloc((size_t)cells * sizeof(double));
    img = (unsigned char *)malloc((size_t)cells);
    if (!rates || !img) {
        free(rates);
        free(img);
        test_record(0, "sa", "param_scan_heatmap", "oom");
        return 0;
    }

    fp = fopen("results/sa_param_scan.csv", "w");
    if (fp)
        fprintf(fp, "alpha,inner,T0,step,n_runs,hit_rate,avg_fevals\n");

    for (it = 0; it < nt; ++it) {
        for (ia = 0; ia < na; ++ia) {
            for (ii = 0; ii < ni; ++ii) {
                sa_params p;
                double rate;
                int avg_fe = 0;
                int idx = (it * na + ia) * ni + ii;
                p.alpha = alphas[ia];
                p.inner = inners[ii];
                p.step = step;
                p.T0 = T0s[it];
                rate = run_rastrigin_rate(&p, 0, 81000 + (unsigned)idx * 17u,
                                          n_runs, &avg_fe);
                rates[idx] = rate;
                img[idx] = (unsigned char)(rate * 255.0 + 0.5);
                ++filled;
                if (fp)
                    fprintf(fp, "%.3f,%d,%.3f,%.3f,%d,%.4f,%d\n",
                            p.alpha, p.inner, p.T0, p.step, n_runs, rate,
                            avg_fe);
            }
        }
    }
    if (fp) fclose(fp);

    pgm = fopen("results/sa_param_scan.pgm", "w");
    if (pgm) {
        int r, c;
        fprintf(pgm, "P2\n%d %d\n255\n", na * ni, nt);
        for (r = 0; r < nt; ++r) {
            for (c = 0; c < na * ni; ++c)
                fprintf(pgm, "%d%s", img[r * na * ni + c],
                        (c + 1 == na * ni) ? "\n" : " ");
        }
        fclose(pgm);
    }
    ok_grid = filled == cells;

    {
        const sa_params mid = {0.95, 20, 0.20, 1.0};
        double r1a, r1b, r2a, r2b;
        r1a = run_rastrigin_rate(&k_tuned, 0, 91001, 400, NULL);
        r1b = run_rastrigin_rate(&k_tuned, 0, 92003, 400, NULL);
        r2a = run_rastrigin_rate(&mid, 0, 93001, 400, NULL);
        r2b = run_rastrigin_rate(&mid, 0, 94003, 400, NULL);
        ok_repro = fabs(r1a - r1b) < 0.05 && fabs(r2a - r2b) < 0.05;
        snprintf(detail, sizeof detail,
                 "cells=%d/%d tuned|%.3f-%.3f|=%.4f mid|%.3f-%.3f|=%.4f (need<0.05)",
                 filled, cells, r1a, r1b, fabs(r1a - r1b),
                 r2a, r2b, fabs(r2a - r2b));
        fp = fopen("results/sa_hit_rate.csv", "a");
        if (fp) {
            fprintf(fp, "repro_tuned_a,%.3f,%d,%.3f,%.3f,400,%.4f,\n",
                    k_tuned.alpha, k_tuned.inner, k_tuned.step, k_tuned.T0, r1a);
            fprintf(fp, "repro_tuned_b,%.3f,%d,%.3f,%.3f,400,%.4f,\n",
                    k_tuned.alpha, k_tuned.inner, k_tuned.step, k_tuned.T0, r1b);
            fprintf(fp, "repro_mid_a,%.3f,%d,%.3f,%.3f,400,%.4f,\n",
                    mid.alpha, mid.inner, mid.step, mid.T0, r2a);
            fprintf(fp, "repro_mid_b,%.3f,%d,%.3f,%.3f,400,%.4f,\n",
                    mid.alpha, mid.inner, mid.step, mid.T0, r2b);
            fclose(fp);
        }
    }

    test_record(ok_grid && ok_repro, "sa", "param_scan_heatmap", detail);
    free(rates);
    free(img);
    return ok_grid && ok_repro;
}

static int t_sa_accept_curve(void)
{
    double lb[2], ub[2];
    double x0[2] = {3.5, -2.5};
    double *dT, *dacc;
    const int cap = 512;
    mlab_sa_problem prob;
    mlab_sa_config cfg;
    mlab_rng rng;
    mlab_sa_result r;
    int n, i, n_window = 0, ok;
    double acc_first, acc_last;
    double sum_T = 0, sum_acc = 0, sum_T2 = 0, sum_Tacc = 0, var_acc = 0;
    double corr = 0.0;
    char detail[176];
    FILE *fp;

    dT = (double *)malloc((size_t)cap * sizeof(double));
    dacc = (double *)malloc((size_t)cap * sizeof(double));
    if (!dT || !dacc) {
        free(dT);
        free(dacc);
        test_record(0, "sa", "accept_rate_vs_temperature", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(2, lb, ub);
    prob.dim = 2;
    prob.f = mlab_rastrigin;
    prob.ctx = NULL;
    prob.lb = lb;
    prob.ub = ub;

    memset(&cfg, 0, sizeof cfg);
    /*
     * 固定邻域幅度：经典 Metropolis 降温下接受率应随 T 下降。
     * （若 σ∝T，低温步长极小→近似贪心全接受，曲线反而回升。）
     */
    cfg.T0 = 12.0;
    cfg.alpha = 0.92;
    cfg.inner = 40;
    cfg.T_min = 0.05;
    cfg.step_scale = 0.15;
    cfg.calibrate_T0 = 0;
    cfg.target_accept = 0.8;
    cfg.max_outer = 60;
    cfg.step_fixed = 1;

    memset(&r, 0, sizeof r);
    mlab_rng_seed_kind(&rng, 77001, MLAB_RNG_SPLITMIX64);
    if (!mlab_sa_continuous_diag(&rng, &prob, &cfg, x0, &r, dT, dacc, cap)) {
        free(dT);
        free(dacc);
        test_record(0, "sa", "accept_rate_vs_temperature", "run fail");
        return 0;
    }
    n = r.n_outer < cap ? r.n_outer : cap;
    if (n < 10) {
        mlab_sa_result_free(&r);
        free(dT);
        free(dacc);
        test_record(0, "sa", "accept_rate_vs_temperature", "too few layers");
        return 0;
    }
    acc_first = dacc[0];
    acc_last = dacc[n - 1];
    for (i = 0; i < n; ++i) {
        sum_T += dT[i];
        sum_acc += dacc[i];
        sum_T2 += dT[i] * dT[i];
        sum_Tacc += dT[i] * dacc[i];
        if (dacc[i] >= 0.15 && dacc[i] <= 0.85) ++n_window;
    }
    {
        double mean_acc = sum_acc / n;
        double mean_T = sum_T / n;
        double cov = sum_Tacc / n - mean_T * mean_acc;
        double var_T = sum_T2 / n - mean_T * mean_T;
        for (i = 0; i < n; ++i) {
            double da = dacc[i] - mean_acc;
            var_acc += da * da;
        }
        var_acc /= n;
        if (var_T > 0 && var_acc > 0)
            corr = cov / sqrt(var_T * var_acc);
    }
    ok = acc_first > acc_last + 0.05
         && corr > 0.3
         && n_window >= 3
         && r.accept_rate > 0.02;
    snprintf(detail, sizeof detail,
             "acc0=%.3f accN=%.3f corr(T,acc)=%.3f window=%d layers=%d overall_acc=%.3f",
             acc_first, acc_last, corr, n_window, n, r.accept_rate);
    test_record(ok, "sa", "accept_rate_vs_temperature", detail);
    fp = fopen("results/sa_accept_curve.csv", "w");
    if (fp) {
        fprintf(fp, "layer,T,accept_rate\n");
        for (i = 0; i < n; ++i)
            fprintf(fp, "%d,%.6f,%.4f\n", i, dT[i], dacc[i]);
        fclose(fp);
    }
    mlab_sa_result_free(&r);
    free(dT);
    free(dacc);
    return ok;
}

/* ---------- suite: tsp ---------- */

static int t_tsp_swap_vs_2opt(void)
{
    const int n_cities = 15;
    const int n_pairs = 50;
    const int budget_outer = 250, budget_inner = 20;
    mlab_tsp_inst inst;
    mlab_tsp_sa_config cfg_s, cfg_2;
    double *len_s, *len_2, *diff;
    int *tour0;
    int i, better = 0;
    double mean_s = 0, mean_2 = 0, mean_d, p;
    int ok;
    char detail[192];
    FILE *fp;

    len_s = (double *)malloc((size_t)n_pairs * sizeof(double));
    len_2 = (double *)malloc((size_t)n_pairs * sizeof(double));
    diff = (double *)malloc((size_t)n_pairs * sizeof(double));
    tour0 = (int *)malloc((size_t)n_cities * sizeof(int));
    if (!len_s || !len_2 || !diff || !tour0) {
        free(len_s); free(len_2); free(diff); free(tour0);
        test_record(0, "tsp", "swap_vs_2opt_paired", "oom");
        return 0;
    }

    memset(&inst, 0, sizeof inst);
    {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 33001, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            free(len_s); free(len_2); free(diff); free(tour0);
            test_record(0, "tsp", "swap_vs_2opt_paired", "inst fail");
            return 0;
        }
        mlab_tsp_shuffle_tour(&rng, n_cities, tour0);
    }

    cfg_s.T0 = 2.0;
    cfg_s.alpha = 0.95;
    cfg_s.T_min = 1e-3;
    cfg_s.inner = budget_inner;
    cfg_s.max_outer = budget_outer;
    cfg_s.nbhd = MLAB_TSP_NBHD_SWAP;
    cfg_2 = cfg_s;
    cfg_2.nbhd = MLAB_TSP_NBHD_2OPT;

    for (i = 0; i < n_pairs; ++i) {
        mlab_rng rs, r2;
        mlab_tsp_sa_result out_s, out_2;
        unsigned seed = 44000u + (unsigned)i * 31u;
        memset(&out_s, 0, sizeof out_s);
        memset(&out_2, 0, sizeof out_2);
        mlab_rng_seed_kind(&rs, seed, MLAB_RNG_SPLITMIX64);
        mlab_rng_seed_kind(&r2, seed, MLAB_RNG_SPLITMIX64);
        if (!mlab_sa_tsp(&rs, &inst, &cfg_s, tour0, &out_s) ||
            !mlab_sa_tsp(&r2, &inst, &cfg_2, tour0, &out_2)) {
            mlab_tsp_sa_result_free(&out_s);
            mlab_tsp_sa_result_free(&out_2);
            len_s[i] = len_2[i] = diff[i] = 0.0;
            continue;
        }
        len_s[i] = out_s.best_len;
        len_2[i] = out_2.best_len;
        diff[i] = len_s[i] - len_2[i];
        if (diff[i] > 0) ++better;
        mean_s += len_s[i];
        mean_2 += len_2[i];
        mlab_tsp_sa_result_free(&out_s);
        mlab_tsp_sa_result_free(&out_2);
    }
    mean_s /= n_pairs;
    mean_2 /= n_pairs;
    mean_d = mean_s - mean_2;
    p = paired_p_two_sided(len_s, len_2, n_pairs);
    /* 路线图：2-opt 最终回路显著更优（配对检验 p<0.01） */
    ok = mean_d > 0 && p < 0.01;
    snprintf(detail, sizeof detail,
             "swap=%.4f 2opt=%.4f Δ=%.4f paired_p=%.3e wins_2opt=%d/%d (p<0.01)",
             mean_s, mean_2, mean_d, p, better, n_pairs);
    test_record(ok, "tsp", "swap_vs_2opt_paired", detail);

    fp = fopen("results/tsp_swap_vs_2opt.csv", "w");
    if (fp) {
        fprintf(fp, "pair,len_swap,len_2opt,diff\n");
        for (i = 0; i < n_pairs; ++i)
            fprintf(fp, "%d,%.6f,%.6f,%.6f\n", i, len_s[i], len_2[i], diff[i]);
        fclose(fp);
    }
    fp = fopen("results/tsp_instance.csv", "w");
    if (fp) {
        fprintf(fp, "id,x,y\n");
        for (i = 0; i < n_cities; ++i)
            fprintf(fp, "%d,%.6f,%.6f\n", i, inst.x[i], inst.y[i]);
        fclose(fp);
    }
    mlab_tsp_free(&inst);
    free(len_s); free(len_2); free(diff); free(tour0);
    return ok;
}

static int t_tsp_sa_2opt_beats_random_walk(void)
{
    const int n_cities = 12;
    const int n_rep = 30;
    const int budget = 200 * 15;
    mlab_tsp_inst inst;
    mlab_tsp_sa_config cfg;
    double sum_sa = 0, sum_rw = 0;
    int i, ok;
    char detail[144];

    memset(&inst, 0, sizeof inst);
    {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 35001, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            test_record(0, "tsp", "sa_2opt_beats_random_walk", "inst fail");
            return 0;
        }
    }
    cfg.T0 = 2.0;
    cfg.alpha = 0.95;
    cfg.T_min = 1e-3;
    cfg.inner = 15;
    cfg.max_outer = 200;
    cfg.nbhd = MLAB_TSP_NBHD_2OPT;

    for (i = 0; i < n_rep; ++i) {
        mlab_rng rs, rrw;
        mlab_tsp_sa_result out;
        int *tour0 = (int *)malloc((size_t)n_cities * sizeof(int));
        int *cur = (int *)malloc((size_t)n_cities * sizeof(int));
        int k;
        double best;
        if (!tour0 || !cur) {
            free(tour0);
            free(cur);
            continue;
        }
        memset(&out, 0, sizeof out);
        mlab_rng_seed_kind(&rs, 36000u + (unsigned)i * 17u, MLAB_RNG_SPLITMIX64);
        mlab_tsp_shuffle_tour(&rs, n_cities, tour0);

        /* 对照：同邻域恒接受随机行走，记历史最优 */
        mlab_rng_seed_kind(&rrw, 38000u + (unsigned)i * 17u, MLAB_RNG_SPLITMIX64);
        memcpy(cur, tour0, (size_t)n_cities * sizeof(int));
        best = mlab_tsp_length(&inst, cur);
        for (k = 0; k < budget; ++k) {
            int a = (int)(mlab_rng_uniform(&rrw) * n_cities);
            int b = (int)(mlab_rng_uniform(&rrw) * n_cities);
            double lf;
            int lo, hi;
            if (a == b) continue;
            lo = a < b ? a : b;
            hi = a < b ? b : a;
            /* 与 SA 提议一致：跳过 2-opt 恒等/长度不变移动，预算口径对齐 */
            if (hi == lo + 1 || (lo == 0 && hi == n_cities - 1)) continue;
            mlab_tsp_apply_2opt(cur, n_cities, a, b);
            lf = mlab_tsp_length(&inst, cur);
            if (lf < best) best = lf;
        }

        if (!mlab_sa_tsp(&rs, &inst, &cfg, tour0, &out)) {
            free(tour0);
            free(cur);
            continue;
        }
        sum_sa += out.best_len;
        sum_rw += best;
        mlab_tsp_sa_result_free(&out);
        free(tour0);
        free(cur);
    }
    sum_sa /= n_rep;
    sum_rw /= n_rep;
    ok = sum_sa < sum_rw * 0.95;
    snprintf(detail, sizeof detail,
             "sa2opt=%.4f random_walk_best=%.4f ratio=%.3f",
             sum_sa, sum_rw, sum_rw > 0 ? sum_sa / sum_rw : 0);
    test_record(ok, "tsp", "sa_2opt_beats_random_walk", detail);
    mlab_tsp_free(&inst);
    return ok;
}

/* ---------- suite: sa — 标定与调度 ---------- */

/*
 * T0 标定（路线图 :412/415）：目标接受率反推 T0。
 * 方法验证：目标 0.8 → 首层接受率落在 0.8±0.25；
 * 且目标调低（0.3）后实测接受率应显著下降（标定旋钮有效）。
 */
static int t_sa_t0_calibration_match(void)
{
    double lb[2], ub[2];
    mlab_sa_problem prob;
    mlab_rng rng;
    const int n_runs = 100;
    const double targets[2] = {0.8, 0.3};
    double acc0[2], T0bar[2];
    int t, i, ok;
    char detail[160];
    FILE *fp;

    mlab_rastrigin_bounds(2, lb, ub);
    prob.dim = 2;
    prob.f = mlab_rastrigin;
    prob.ctx = NULL;
    prob.lb = lb;
    prob.ub = ub;
    fp = fopen("results/sa_t0_calibration.csv", "w");
    if (fp)
        fprintf(fp, "target,T0_used,acc_first_layer\n");
    for (t = 0; t < 2; ++t) {
        double sum_acc = 0.0, sum_T0 = 0.0;
        for (i = 0; i < n_runs; ++i) {
            mlab_sa_config cfg;
            mlab_sa_result r;
            double x0[2], dT[64], dacc[64];
            memset(&cfg, 0, sizeof cfg);
            cfg.T0 = 1.0;                 /* 仅作 calibrate 失败时的兜底 */
            cfg.alpha = k_tuned.alpha;
            cfg.inner = k_tuned.inner;
            cfg.T_min = 1e-4;
            cfg.step_scale = k_tuned.step;
            cfg.calibrate_T0 = 1;
            cfg.target_accept = targets[t];
            cfg.max_outer = 0;
            cfg.step_fixed = 0;
            mlab_rng_seed_kind(&rng, 74500u + (unsigned)t * 100000u
                                          + (unsigned)i * 23u,
                               MLAB_RNG_SPLITMIX64);
            x0[0] = lb[0] + mlab_rng_uniform(&rng) * (ub[0] - lb[0]);
            x0[1] = lb[1] + mlab_rng_uniform(&rng) * (ub[1] - lb[1]);
            memset(&r, 0, sizeof r);
            if (!mlab_sa_continuous_diag(&rng, &prob, &cfg, x0, &r,
                                         dT, dacc, 64)) {
                mlab_sa_result_free(&r);
                continue;
            }
            sum_acc += dacc[0];
            sum_T0 += r.T0_used;
            if (fp)
                fprintf(fp, "%.2f,%.6f,%.4f\n", targets[t], r.T0_used, dacc[0]);
            mlab_sa_result_free(&r);
        }
        acc0[t] = sum_acc / n_runs;
        T0bar[t] = sum_T0 / n_runs;
    }
    if (fp) fclose(fp);
    ok = fabs(acc0[0] - 0.8) <= 0.25 && acc0[1] < acc0[0] - 0.2;
    snprintf(detail, sizeof detail,
             "target0.8: T0=%.1f acc=%.3f | target0.3: T0=%.1f acc=%.3f (need acc↓)",
             T0bar[0], acc0[0], T0bar[1], acc0[1]);
    test_record(ok, "sa", "t0_calibration_match", detail);
    return ok;
}

/*
 * 对数调度 / 重加热 / 多重启（路线图 :413 知识点）：
 * 短预算（每跑 600 评估）下：
 *  - 多重启 best-of-8 命中率应显著高于单跑（+15pp 以上）；
 *  - 对数调度（慢降温）与重加热作为对照臂落盘。
 */
static int t_sa_reheat_restart_low_budget(void)
{
    double lb[2], ub[2];
    mlab_sa_problem prob;
    mlab_sa_config cfg;
    const int n_trials = 200, n_restart = 8;
    int i, s_single = 0, s_reheat = 0, s_log = 0, s_multi = 0, ok;
    double p1, pr, pl, pm;
    double fb_single = 0, fb_reheat = 0, fb_log = 0;
    char detail[200];
    FILE *fp;

    mlab_rastrigin_bounds(2, lb, ub);
    prob.dim = 2;
    prob.f = mlab_rastrigin;
    prob.ctx = NULL;
    prob.lb = lb;
    prob.ub = ub;

    memset(&cfg, 0, sizeof cfg);
    /* 与调参配置同形（T0=1, α=0.99, 自动层数≈919），内循环缩到 5 →
     * 每跑约 4.6k 评估：单跑冻结在随机局部盆地，多重启拼预算逃生 */
    cfg.T0 = 1.0;
    cfg.alpha = 0.99;
    cfg.inner = 5;
    cfg.T_min = 1e-4;
    cfg.step_scale = 0.20;
    cfg.max_outer = 0;
    cfg.step_fixed = 0;

    for (i = 0; i < n_trials; ++i) {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 75500u + (unsigned)i * 29u,
                           MLAB_RNG_SPLITMIX64);
        {
            /* 单跑（随机起点） */
            double x0[2];
            mlab_sa_result r;
            memset(&r, 0, sizeof r);
            x0[0] = lb[0] + mlab_rng_uniform(&rng) * (ub[0] - lb[0]);
            x0[1] = lb[1] + mlab_rng_uniform(&rng) * (ub[1] - lb[1]);
            if (mlab_sa_continuous(&rng, &prob, &cfg, x0, &r)) {
                if (rastrigin_hit(r.f_best)) ++s_single;
                fb_single += r.f_best;
                mlab_sa_result_free(&r);
            }
        }
        {
            /* 重加热对照：每 300 层回到 0.5·T0。周期重加热保持流动性、
             * 改善 mean_fbest，但温度不再降到 T_min，精确命中（f<1e-6）归零 */
            double x0[2];
            mlab_sa_config cr = cfg;
            mlab_sa_result r;
            cr.reheat_every = 300;
            cr.reheat_frac = 0.5;
            memset(&r, 0, sizeof r);
            x0[0] = lb[0] + mlab_rng_uniform(&rng) * (ub[0] - lb[0]);
            x0[1] = lb[1] + mlab_rng_uniform(&rng) * (ub[1] - lb[1]);
            if (mlab_sa_continuous(&rng, &prob, &cr, x0, &r)) {
                if (rastrigin_hit(r.f_best)) ++s_reheat;
                fb_reheat += r.f_best;
                mlab_sa_result_free(&r);
            }
        }
        {
            /* 对数调度：同 30 层 */
            double x0[2];
            mlab_sa_config cl = cfg;
            mlab_sa_result r;
            cl.schedule = MLAB_SA_SCHEDULE_LOG;
            memset(&r, 0, sizeof r);
            x0[0] = lb[0] + mlab_rng_uniform(&rng) * (ub[0] - lb[0]);
            x0[1] = lb[1] + mlab_rng_uniform(&rng) * (ub[1] - lb[1]);
            if (mlab_sa_continuous(&rng, &prob, &cl, x0, &r)) {
                if (rastrigin_hit(r.f_best)) ++s_log;
                fb_log += r.f_best;
                mlab_sa_result_free(&r);
            }
        }
        {
            /* 多重启：同一 rng 流继续产生 8 个随机起点，best-of */
            mlab_sa_result r;
            memset(&r, 0, sizeof r);
            if (mlab_sa_multirestart(&rng, &prob, &cfg, n_restart, &r)) {
                if (rastrigin_hit(r.f_best)) ++s_multi;
                mlab_sa_result_free(&r);
            }
        }
    }
    p1 = (double)s_single / n_trials;
    pr = (double)s_reheat / n_trials;
    pl = (double)s_log / n_trials;
    pm = (double)s_multi / n_trials;
    ok = pm > p1 + 0.15;
    fp = fopen("results/sa_reheat_restart.csv", "w");
    if (fp) {
        fprintf(fp, "arm,hit_rate,mean_fbest,trials,evals_per_run\n");
        fprintf(fp, "single,%.4f,%.4f,%d,%d\n", p1, fb_single / n_trials,
                n_trials, cfg.inner * 920);
        fprintf(fp, "reheat_300x0.3,%.4f,%.4f,%d,%d\n", pr,
                fb_reheat / n_trials, n_trials, cfg.inner * 920);
        fprintf(fp, "log_schedule,%.4f,%.4f,%d,%d\n", pl,
                fb_log / n_trials, n_trials, cfg.inner * 920);
        fprintf(fp, "multirestart8,%.4f,,%d,%d\n", pm, n_trials,
                cfg.inner * 920);
        fclose(fp);
    }
    snprintf(detail, sizeof detail,
             "single=%.3f reheat=%.3f log=%.3f multi=%.3f (need multi>single+0.15)",
             p1, pr, pl, pm);
    test_record(ok, "sa", "reheat_restart_low_budget", detail);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"sa", "rastrigin_tuned_1000_runs",
         "调参后 1000 次 2D Rastrigin 命中率≥80%",
         t_sa_rastrigin_tuned_1000},
        {"sa", "param_scan_heatmap",
         "α×inner×T0 网格热图完整 + 同参复跑±5pp",
         t_sa_param_scan_heatmap},
        {"sa", "accept_rate_vs_temperature",
         "接受率随温度下降；有效降温窗口",
         t_sa_accept_curve},
        {"sa", "t0_calibration_match",
         "T0 标定：首层接受率落到目标 0.8 附近",
         t_sa_t0_calibration_match},
        {"sa", "reheat_restart_low_budget",
         "短预算下多重启 best-of-8 优于单跑；重加热/对数调度对照",
         t_sa_reheat_restart_low_budget},
        {"tsp", "swap_vs_2opt_paired",
         "同预算 swap vs 2-opt 配对检验 p<0.01",
         t_tsp_swap_vs_2opt},
        {"tsp", "sa_2opt_beats_random_walk",
         "SA-2opt 优于恒接受随机行走基线",
         t_tsp_sa_2opt_beats_random_walk},
    };
    test_ensure_results_dir();
    return test_run_main("C3-sa", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
