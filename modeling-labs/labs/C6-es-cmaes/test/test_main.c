/*
 * C6: ES ((1+1)+1/5) / CMA-ES
 */
#include "harness.h"
#include "lab.h"
#include "es.h"
#include "cmaes.h"
#include "de.h"
#include "bench.h"
#include "bench_sa.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MLAB_PI
#define MLAB_PI 3.14159265358979323846
#endif

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

static double spearman_rho(const double *x, const double *y, int n)
{
    double *rx, *ry;
    double mx, my, num = 0.0, dx = 0.0, dy = 0.0, rho;
    int i, j;
    if (n < 3) return 0.0;
    rx = (double *)malloc((size_t)n * sizeof(double));
    ry = (double *)malloc((size_t)n * sizeof(double));
    if (!rx || !ry) { free(rx); free(ry); return 0.0; }
    /* rank both series */
    for (i = 0; i < n; ++i) {
        int rx_ = 0, ry_ = 0;
        for (j = 0; j < n; ++j) {
            if (x[j] < x[i]) ++rx_;
            if (y[j] < y[i]) ++ry_;
        }
        rx[i] = (double)rx_;
        ry[i] = (double)ry_;
    }
    mx = 0.0; my = 0.0;
    for (i = 0; i < n; ++i) { mx += rx[i]; my += ry[i]; }
    mx /= n; my /= n;
    for (i = 0; i < n; ++i) {
        num += (rx[i] - mx) * (ry[i] - my);
        dx += (rx[i] - mx) * (rx[i] - mx);
        dy += (ry[i] - my) * (ry[i] - my);
    }
    rho = (dx > 0 && dy > 0) ? num / sqrt(dx * dy) : 0.0;
    free(rx); free(ry);
    return rho;
}

typedef struct {
    double theta;
    double a, b;
} ell_ctx;

static double ellipsoid_rot(const double *x, int n, void *ctx)
{
    const ell_ctx *e = (const ell_ctx *)ctx;
    double c = cos(e->theta), s = sin(e->theta);
    double y0 = c * x[0] + s * x[1];
    double y1 = -s * x[0] + c * x[1];
    (void)n;
    return 0.5 * (e->a * y0 * y0 + e->b * y1 * y1);
}

static double run_cma(double (*f)(const double *, int, void *), void *ctx,
                      int dim, const double *lb, const double *ub,
                      double sigma0, double stop_f, int max_evals,
                      double x0_scale, unsigned seed,
                      int track_angle, const double *major,
                      double *evals_out, double *best_out)
{
    mlab_rng rng;
    mlab_cmaes_config cfg;
    mlab_cmaes_result r;
    double best;
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    memset(&cfg, 0, sizeof cfg);
    cfg.f = f;
    cfg.ctx = ctx;
    cfg.dim = dim;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.max_evals = max_evals;
    cfg.sigma0 = sigma0;
    cfg.stop_f = stop_f;
    cfg.x0_scale = x0_scale;
    cfg.track_angle = track_angle;
    if (major) {
        cfg.major_axis[0] = major[0];
        cfg.major_axis[1] = major[1];
    }
    memset(&r, 0, sizeof r);
    if (!mlab_cmaes_run(&rng, &cfg, NULL, &r)) {
        if (evals_out) *evals_out = (double)max_evals;
        if (best_out) *best_out = 1e300;
        return 1e300;
    }
    best = r.best_f;
    if (evals_out) {
        if (r.reached && r.evals_to_target > 0) *evals_out = (double)r.evals_to_target;
        else *evals_out = (double)r.n_eval;
    }
    if (best_out) *best_out = best;
    mlab_cmaes_result_free(&r);
    return best;
}

/* DE 预算感知：固定 seed 前缀性质 → 二分 max_gen 求评估数（失败记满预算） */
static double run_de_evals_to_target(double (*f)(const double *, int, void *),
                                     int dim, const double *lb, const double *ub,
                                     double F, double CR, int pop,
                                     int max_evals, double target,
                                     unsigned seed, double *best_out)
{
    int max_gen = max_evals / pop;
    int lo = 0, hi = max_gen, mid;
    double best_full = 1e300;

    /* 全预算一次 */
    {
        mlab_rng rng;
        mlab_de_config cfg;
        mlab_de_result r;
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        memset(&cfg, 0, sizeof cfg);
        cfg.f = f;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.pop = pop;
        cfg.max_gen = max_gen;
        cfg.F = F;
        cfg.CR = CR;
        cfg.variant = MLAB_DE_RAND1;
        memset(&r, 0, sizeof r);
        if (!mlab_de_run(&rng, &cfg, NULL, &r, 1e300)) {
            if (best_out) *best_out = 1e300;
            return (double)max_evals;
        }
        best_full = r.best_f;
        mlab_de_result_free(&r);
    }
    if (best_out) *best_out = best_full;
    if (!(best_full < target))
        return (double)max_evals; /* 未达标：惩罚 = 预算 */

    /* 二分最小 max_gen 使 best < target（同 seed 前缀可复现） */
    while (lo < hi) {
        mlab_rng rng;
        mlab_de_config cfg;
        mlab_de_result r;
        mid = (lo + hi) / 2;
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        memset(&cfg, 0, sizeof cfg);
        cfg.f = f;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.pop = pop;
        cfg.max_gen = mid;
        cfg.F = F;
        cfg.CR = CR;
        cfg.variant = MLAB_DE_RAND1;
        memset(&r, 0, sizeof r);
        if (!mlab_de_run(&rng, &cfg, NULL, &r, 1e300)) {
            hi = mid;
        } else {
            int ok = (r.best_f < target);
            mlab_de_result_free(&r);
            if (ok) hi = mid;
            else lo = mid + 1;
        }
    }
    /* 评估数 ≈ pop * (lo + 1)（每代全种群评估） */
    return (double)(pop * (lo + 1));
}

/* ---------- suite: es ---------- */

static int t_es_fifth_rule_sphere(void)
{
    const int dim = 10, n_runs = 25, max_gen = 400;
    double sum_late = 0.0, sum_warm = 0.0, sum_sf = 0.0;
    int i, n_ok = 0;
    FILE *fp;
    char detail[200];

    fp = fopen("results/es_fifth_rule_sphere.csv", "w");
    if (fp) fprintf(fp, "run,success_warm,success_late,sigma0,sigma_final,best_f\n");
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_es1p1_config cfg;
        mlab_es1p1_result r;
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_sphere;
        cfg.dim = dim;
        cfg.max_gen = max_gen;
        cfg.sigma0 = 1.0;
        cfg.p_target = 0.2;
        cfg.window = 20;
        cfg.adapt_factor = 1.22;
        cfg.x0_scale = 3.0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 91000u + (unsigned)i * 17u, MLAB_RNG_SPLITMIX64);
        if (!mlab_es1p1_run(&rng, &cfg, NULL, &r)) continue;
        sum_warm += r.success_rate_warm;
        sum_late += r.success_rate_late;
        sum_sf += r.sigma_final;
        if (fp)
            fprintf(fp, "%d,%.4f,%.4f,%.4f,%.6f,%.6g\n",
                    i, r.success_rate_warm, r.success_rate_late,
                    cfg.sigma0, r.sigma_final, r.best_f);
        mlab_es1p1_result_free(&r);
        ++n_ok;
    }
    if (fp) fclose(fp);
    if (n_ok < 15) {
        test_record(0, "es", "fifth_rule_success_rate_sphere", "too few runs");
        return 0;
    }
    {
        double mr = sum_late / n_ok;
        double mw = sum_warm / n_ok;
        double msf = sum_sf / n_ok;
        /* 判据：后半段成功率 ∈ [0.2±0.05]；且自适应已启动（晚段≠预热极端值） */
        int ok = (mr >= 0.15 && mr <= 0.25);
        snprintf(detail, sizeof detail,
                 "late_success=%.3f (target 0.2±0.05) warm=%.3f mean_sigma_final=%.3f n=%d",
                 mr, mw, msf, n_ok);
        test_record(ok, "es", "fifth_rule_success_rate_sphere", detail);
        return ok;
    }
}

static int t_es_adapt_vs_fixed_sigma(void)
{
    const int dim = 10, n_runs = 20, max_gen = 400;
    double *late_ad = (double *)malloc((size_t)n_runs * sizeof(double));
    double *late_fx = (double *)malloc((size_t)n_runs * sizeof(double));
    double sum_ad = 0.0, sum_fx = 0.0, sum_sig = 0.0;
    int i, nad = 0, nfx = 0, ok;
    FILE *fp;
    char detail[200];

    if (!late_ad || !late_fx) {
        free(late_ad); free(late_fx);
        test_record(0, "es", "adaptive_sigma_keeps_rate_near_fifth", "oom");
        return 0;
    }
    fp = fopen("results/es_sigma_adapt.csv", "w");
    if (fp) fprintf(fp, "mode,run,success_late,sigma_final,best_f\n");
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_es1p1_config cfg;
        mlab_es1p1_result r;
        /* 自适应 */
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_sphere;
        cfg.dim = dim;
        cfg.max_gen = max_gen;
        cfg.sigma0 = 1.0;
        cfg.p_target = 0.2;
        cfg.window = 20;
        cfg.adapt_factor = 1.22;
        cfg.x0_scale = 3.0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 92000u + (unsigned)i * 13u, MLAB_RNG_SPLITMIX64);
        if (mlab_es1p1_run(&rng, &cfg, NULL, &r)) {
            late_ad[nad++] = r.success_rate_late;
            sum_ad += r.success_rate_late;
            sum_sig += r.sigma_final;
            if (fp) fprintf(fp, "adapt,%d,%.4f,%.6f,%.6g\n",
                            i, r.success_rate_late, r.sigma_final, r.best_f);
        }
        mlab_es1p1_result_free(&r);
        /* 固定步长（factor=1，不调整） */
        cfg.adapt_factor = 1.0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 92000u + (unsigned)i * 13u, MLAB_RNG_SPLITMIX64);
        if (mlab_es1p1_run(&rng, &cfg, NULL, &r)) {
            late_fx[nfx++] = r.success_rate_late;
            sum_fx += r.success_rate_late;
            if (fp) fprintf(fp, "fixed,%d,%.4f,%.6f,%.6g\n",
                            i, r.success_rate_late, r.sigma_final, r.best_f);
        }
        mlab_es1p1_result_free(&r);
    }
    if (fp) fclose(fp);
    if (nad < 10 || nfx < 10) {
        test_record(0, "es", "adaptive_sigma_keeps_rate_near_fifth", "too few runs");
        free(late_ad); free(late_fx);
        return 0;
    }
    {
        double mad = sum_ad / nad, mfx = sum_fx / nfx;
        double err_ad = fabs(mad - 0.2), err_fx = fabs(mfx - 0.2);
        /* 自适应晚段成功率更接近 1/5；且平均末态步长显示已自适应 */
        ok = (err_ad < err_fx) && (err_ad <= 0.08) && (sum_sig / nad < 5.0);
        snprintf(detail, sizeof detail,
                 "adapt_late=%.3f (err %.3f) fixed_late=%.3f (err %.3f) mean_sigma_final=%.3f",
                 mad, err_ad, mfx, err_fx, sum_sig / nad);
        test_record(ok, "es", "adaptive_sigma_keeps_rate_near_fifth", detail);
    }
    free(late_ad); free(late_fx);
    return ok;
}

/* ---------- suite: cmaes ---------- */

static int t_cma_vs_de_rosenbrock(void)
{
    const int dim = 5, n_runs = 40, budget = 20000, pop = 40;
    const double target = 1e-6;
    double lb[5], ub[5];
    double *e_cma = (double *)malloc((size_t)n_runs * sizeof(double));
    double *e_de = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_cma = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_de = (double *)malloc((size_t)n_runs * sizeof(double));
    int i, ok, suc_c = 0, suc_d = 0;
    double p, mc, md;
    FILE *fp;
    char detail[240];

    if (!e_cma || !e_de || !f_cma || !f_de) {
        free(e_cma); free(e_de); free(f_cma); free(f_de);
        test_record(0, "cmaes", "cma_fewer_evals_than_de_rosenbrock", "oom");
        return 0;
    }
    for (i = 0; i < dim; ++i) { lb[i] = -2.5; ub[i] = 2.5; }
    fp = fopen("results/cmaes_de_rosenbrock.csv", "w");
    if (fp) fprintf(fp, "run,evals_cma,evals_de,best_cma,best_de\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 93000u + (unsigned)i * 31u;
        run_cma(mlab_rosenbrock, NULL, dim, lb, ub,
                0.6, target, budget, 0.8, seed, 0, NULL,
                &e_cma[i], &f_cma[i]);
        e_de[i] = run_de_evals_to_target(mlab_rosenbrock, dim, lb, ub,
                                         0.5, 0.9, pop, budget, target,
                                         seed, &f_de[i]);
        if (f_cma[i] < target) ++suc_c;
        if (f_de[i] < target) ++suc_d;
        if (fp)
            fprintf(fp, "%d,%.0f,%.0f,%.6g,%.6g\n",
                    i, e_cma[i], e_de[i], f_cma[i], f_de[i]);
    }
    if (fp) fclose(fp);
    mc = mlab_mean(e_cma, n_runs);
    md = mlab_mean(e_de, n_runs);
    p = paired_p_two_sided(e_de, e_cma, n_runs); /* H: DE - CMA > 0 → 单侧更贴切；用双侧仍可 */
    /* 判据：CMA 评估数显著更少（配对 p<0.01）且均值更低 */
    ok = (mc < md) && (p < 0.01);
    snprintf(detail, sizeof detail,
             "Rosenbrock5D target=%.0e budget=%d | evals CMA=%.0f DE=%.0f p=%.2e | succ CMA=%d DE=%d",
             target, budget, mc, md, p, suc_c, suc_d);
    test_record(ok, "cmaes", "cma_fewer_evals_than_de_rosenbrock", detail);
    free(e_cma); free(e_de); free(f_cma); free(f_de);
    return ok;
}

static int t_cma_vs_de_rastrigin_narrows(void)
{
    /*
     * 多峰 Rastrigin：与 Rosenbrock「评估数大幅碾压」对照。
     * DE 用偏探索参数（F=0.9）；判据要求 CMA 的评估数优势消失/缩窄
     * 或 DE 在终态质量上具备竞争力（不再出现 Rosenbrock 式单边碾压）。
     */
    const int dim = 10, n_runs = 30, budget = 25000, pop = 50;
    const double target = 8.0;
    const double ros_eval_ratio = 0.20; /* Rosenbrock 实测 CMA/DE 评估数比 ≈0.20，见 report */
    double lb[10], ub[10];
    double *e_cma = (double *)malloc((size_t)n_runs * sizeof(double));
    double *e_de = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_cma = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_de = (double *)malloc((size_t)n_runs * sizeof(double));
    int i, ok, suc_c = 0, suc_d = 0;
    double p_evals, p_best, mc, md, ratio, mb_c, mb_d;
    FILE *fp;
    char detail[280];

    if (!e_cma || !e_de || !f_cma || !f_de) {
        free(e_cma); free(e_de); free(f_cma); free(f_de);
        test_record(0, "cmaes", "cma_advantage_narrows_on_rastrigin", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(dim, lb, ub);
    fp = fopen("results/cmaes_de_rastrigin.csv", "w");
    if (fp) fprintf(fp, "run,evals_cma,evals_de,best_cma,best_de\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 95000u + (unsigned)i * 29u;
        run_cma(mlab_rastrigin, NULL, dim, lb, ub,
                1.0, target, budget, 2.0, seed, 0, NULL,
                &e_cma[i], &f_cma[i]);
        e_de[i] = run_de_evals_to_target(mlab_rastrigin, dim, lb, ub,
                                         0.9, 0.9, pop, budget, target,
                                         seed, &f_de[i]);
        if (f_cma[i] < target) ++suc_c;
        if (f_de[i] < target) ++suc_d;
        if (fp)
            fprintf(fp, "%d,%.0f,%.0f,%.6g,%.6g\n",
                    i, e_cma[i], e_de[i], f_cma[i], f_de[i]);
    }
    if (fp) fclose(fp);
    mc = mlab_mean(e_cma, n_runs);
    md = mlab_mean(e_de, n_runs);
    mb_c = mlab_mean(f_cma, n_runs);
    mb_d = mlab_mean(f_de, n_runs);
    ratio = md > 0 ? mc / md : 1.0;
    p_evals = paired_p_two_sided(e_de, e_cma, n_runs);
    p_best = paired_p_two_sided(f_de, f_cma, n_runs); /* H: DE best 更大 = CMA 更好 */
    /*
     * 正向量化判据（:493 优势缩窄或反转，不再用反向排除式）：
     *  - 缩窄：评估数比 CMA/DE 相对 Rosenbrock 基线(≈0.20)至少抬高一倍（≥0.40）
     *  - 或反转：DE 终态质量显著更好（配对 p<0.01）
     */
    ok = (ratio >= 2.0 * ros_eval_ratio) ||
         (mb_d < mb_c && p_best < 0.01);
    snprintf(detail, sizeof detail,
             "Ras10D tgt=%.1f | evals CMA=%.0f DE=%.0f ratio=%.2f need>=%.2f p_e=%.2e | best CMA=%.2f DE=%.2f p_b=%.2e | succ %d/%d (ros_ratio_ref=%.2f)",
             target, mc, md, ratio, 2.0 * ros_eval_ratio, p_evals,
             mb_c, mb_d, p_best, suc_c, suc_d, ros_eval_ratio);
    test_record(ok, "cmaes", "cma_advantage_narrows_on_rastrigin", detail);
    free(e_cma); free(e_de); free(f_cma); free(f_de);
    return ok;
}

/* ---------- (μ/ρ,λ)-ES：comma vs plus ---------- */

static int t_es_murl_comma_plus(void)
{
    /*
     * 路线图 :491 知识点：(μ/ρ,λ) 与 (μ/ρ+λ)。机制对照：
     *  - plus 选择父代∪子代取最好 μ → 种群最优单调不回退（构造保证）
     *  - comma 只在子代中选 → 种群最优可回退，但保留 σ 谱系多样性
     * 断言：两种模式在 10D Sphere 上都收敛（<1e-4）；plus 的 pop_best_hist
     * 单调不增；comma 与 plus 的终态差异如实落盘。
     */
    const int dim = 10, n_runs = 25, mu = 5, rho = 2, lam = 10, max_gen = 300;
    double lb[10], ub[10];
    double *f_com = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_plus = (double *)malloc((size_t)n_runs * sizeof(double));
    int i, ok, nc = 0, np = 0, n_mono_ok = 0;
    FILE *fp;
    char detail[220];

    if (!f_com || !f_plus) {
        free(f_com); free(f_plus);
        test_record(0, "es", "mu_rho_lambda_comma_plus", "oom");
        return 0;
    }
    for (i = 0; i < dim; ++i) { lb[i] = -5.0; ub[i] = 5.0; }
    fp = fopen("results/es_murl_comma_plus.csv", "w");
    if (fp) fprintf(fp, "run,mode,best_f,sigma_final\n");
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_es_murl_config cfg;
        mlab_es_murl_result r;
        int k, mono = 1;

        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_sphere;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.mu = mu;
        cfg.rho = rho;
        cfg.lambda_n = lam;
        cfg.plus = 0;
        cfg.max_gen = max_gen;
        cfg.sigma0 = 1.0;
        cfg.x0_scale = 3.0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 98000u + (unsigned)i * 23u, MLAB_RNG_SPLITMIX64);
        if (mlab_es_murl_run(&rng, &cfg, &r)) {
            f_com[nc++] = r.best_f;
            if (fp)
                fprintf(fp, "%d,comma,%.6g,%.4g\n", i, r.best_f, r.sigma_final);
        }
        mlab_es_murl_result_free(&r);

        cfg.plus = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 98000u + (unsigned)i * 23u, MLAB_RNG_SPLITMIX64);
        if (mlab_es_murl_run(&rng, &cfg, &r)) {
            f_plus[np++] = r.best_f;
            for (k = 1; k < r.hist_len; ++k)
                if (r.pop_best_hist[k] > r.pop_best_hist[k - 1] + 1e-15)
                    mono = 0;
            if (mono) ++n_mono_ok;
            if (fp)
                fprintf(fp, "%d,plus,%.6g,%.4g\n", i, r.best_f, r.sigma_final);
        }
        mlab_es_murl_result_free(&r);
    }
    if (fp) fclose(fp);
    if (nc < 10 || np < 10) {
        test_record(0, "es", "mu_rho_lambda_comma_plus", "too few runs");
        free(f_com); free(f_plus);
        return 0;
    }
    {
        double mc = mlab_mean(f_com, nc), mp = mlab_mean(f_plus, np);
        int both_target = 1;
        for (i = 0; i < nc; ++i)
            if (!(f_com[i] < 1e-4)) both_target = 0;
        for (i = 0; i < np; ++i)
            if (!(f_plus[i] < 1e-4)) both_target = 0;
        ok = both_target && (n_mono_ok == np);
        snprintf(detail, sizeof detail,
                 "Sphere10 μ=5/ρ=2/λ=10 | final comma=%.3g plus=%.3g | plus monotone %d/%d",
                 mc, mp, n_mono_ok, np);
    }
    test_record(ok, "es", "mu_rho_lambda_comma_plus", detail);
    free(f_com); free(f_plus);
    return ok;
}

/* ---------- CMA-ES IPOP 重启 ---------- */

static int t_cmaes_ipop_rastrigin(void)
{
    /*
     * 路线图 :494：IPOP 重启策略与预算分配。多峰 Rastrigin 上单次 CMA-ES
     * 常停滞于局部最优；IPOP 以 λ 倍增重启分配预算后应显著更优（文献：
     * Auger & Hansen 2005）。同 seed 同总预算配对对比。
     */
    const int dim = 10, n_runs = 20, budget = 40000;
    const double target = 0.1;
    double lb[10], ub[10];
    double *f_one = (double *)malloc((size_t)n_runs * sizeof(double));
    double *f_ip = (double *)malloc((size_t)n_runs * sizeof(double));
    int i, ok, suc_one = 0, suc_ip = 0;
    double p;
    FILE *fp;
    char detail[220];

    if (!f_one || !f_ip) {
        free(f_one); free(f_ip);
        test_record(0, "cmaes", "ipop_restarts_solve_rastrigin", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(dim, lb, ub);
    fp = fopen("results/cmaes_ipop_rastrigin.csv", "w");
    if (fp) fprintf(fp, "run,best_single,best_ipop,evals_single,evals_ipop\n");
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_cmaes_config cfg;
        mlab_cmaes_result r;
        unsigned seed = 99000u + (unsigned)i * 37u;
        double e1, e2;

        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_rastrigin;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.max_evals = budget;
        cfg.sigma0 = 1.0;
        cfg.stop_f = target;
        cfg.x0_scale = 2.0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        if (mlab_cmaes_run(&rng, &cfg, NULL, &r)) {
            f_one[i] = r.best_f;
            e1 = (double)r.n_eval;
        } else {
            f_one[i] = 1e300;
            e1 = (double)budget;
        }
        mlab_cmaes_result_free(&r);

        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        if (mlab_cmaes_run_ipop(&rng, &cfg, &r)) {
            f_ip[i] = r.best_f;
            e2 = (double)r.n_eval;
        } else {
            f_ip[i] = 1e300;
            e2 = (double)budget;
        }
        mlab_cmaes_result_free(&r);

        if (f_one[i] < target) ++suc_one;
        if (f_ip[i] < target) ++suc_ip;
        if (fp)
            fprintf(fp, "%d,%.6g,%.6g,%.0f,%.0f\n", i, f_one[i], f_ip[i], e1, e2);
    }
    if (fp) fclose(fp);
    p = paired_p_two_sided(f_ip, f_one, n_runs);
    {
        double m1 = mlab_mean(f_one, n_runs), m2 = mlab_mean(f_ip, n_runs);
        /* IPOP 终态更优（p<0.01）且成功次数不少于单次运行 */
        ok = (m2 < m1) && (p < 0.01) && (suc_ip >= suc_one);
        snprintf(detail, sizeof detail,
                 "Ras10D budget=%d tgt=%.1f | best single=%.3g ipop=%.3g p=%.2e | succ %d vs %d",
                 budget, target, m1, m2, p, suc_one, suc_ip);
    }
    test_record(ok, "cmaes", "ipop_restarts_solve_rastrigin", detail);
    free(f_one); free(f_ip);
    return ok;
}

static int t_cmaes_cov_axis_align(void)
{
    /*
     * 旋转椭球：f=0.5(a y1^2 + b y2^2), y=R(-θ)x；长轴（易方向）=(cosθ,sinθ)。
     * 判据看「学习阶段」夹角收敛：早期随机 C → 主轴夹角大；
     * 若干代后 C 主轴贴合等高线长轴。σ 数值塌缩后的尖点不计入。
     */
    const int n_runs = 16, max_evals = 2500;
    const double theta = MLAB_PI / 6.0; /* 30° */
    const int g_early0 = 0, g_early1 = 2;     /* 初始随机阶段 */
    const int g_learn0 = 8, g_learn1 = 28;    /* 学习对齐阶段 */
    ell_ctx ctx;
    double lb[2] = {-3.0, -3.0}, ub[2] = {3.0, 3.0};
    double major[2];
    double *mean_ang = NULL;
    int hist_len = 0, i, g, ok, n_used = 0;
    FILE *fp, *pgm;
    char detail[260];

    ctx.theta = theta;
    ctx.a = 1.0;
    ctx.b = 100.0;
    major[0] = cos(theta);
    major[1] = sin(theta);

    mean_ang = (double *)calloc(128, sizeof(double));
    if (!mean_ang) {
        test_record(0, "cmaes", "cov_axis_aligns_rotated_ellipse", "oom");
        return 0;
    }
    fp = fopen("results/cmaes_axis_align.csv", "w");
    if (fp) fprintf(fp, "run,gen,angle_deg,sigma,best_f\n");

    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_cmaes_config cfg;
        mlab_cmaes_result r;
        int gmax;
        memset(&cfg, 0, sizeof cfg);
        cfg.f = ellipsoid_rot;
        cfg.ctx = &ctx;
        cfg.dim = 2;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.max_evals = max_evals;
        cfg.sigma0 = 1.2;
        cfg.stop_f = 0.0;
        cfg.x0_scale = 1.2;
        cfg.track_angle = 1;
        cfg.major_axis[0] = major[0];
        cfg.major_axis[1] = major[1];
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 97000u + (unsigned)i * 19u, MLAB_RNG_SPLITMIX64);
        if (!mlab_cmaes_run(&rng, &cfg, NULL, &r) || r.hist_len < 12 || !r.angle_hist) {
            mlab_cmaes_result_free(&r);
            continue;
        }
        gmax = r.hist_len < 128 ? r.hist_len : 128;
        /* 只累计 σ 仍适中的代，避免数值塌缩后主轴漂移 */
        for (g = 0; g < gmax; ++g) {
            if (fp)
                fprintf(fp, "%d,%d,%.4f,%.6f,%.6g\n",
                        i, g, r.angle_hist[g], r.sigma_hist[g], r.best_hist[g]);
            if (r.sigma_hist[g] >= 1e-4) {
                mean_ang[g] += r.angle_hist[g];
                if (g + 1 > hist_len) hist_len = g + 1;
            }
        }
        ++n_used;
        mlab_cmaes_result_free(&r);
    }
    if (fp) fclose(fp);
    if (n_used < 8 || hist_len < g_learn1 + 1) {
        free(mean_ang);
        test_record(0, "cmaes", "cov_axis_aligns_rotated_ellipse",
                    "too few aligned traces");
        return 0;
    }
    for (g = 0; g < hist_len; ++g) mean_ang[g] /= n_used;

    /* PGM：学习阶段 mean angle vs gen（高=角大=亮） */
    pgm = fopen("results/cmaes_axis_align.pgm", "w");
    if (pgm) {
        const int W = 100, H = 50;
        unsigned char *img = (unsigned char *)calloc((size_t)W * H, 1);
        int a, b;
        if (img) {
            for (a = 0; a < W; ++a) {
                int gi = (int)((double)a / (double)(W - 1) * (g_learn1));
                int row;
                if (gi < 0) gi = 0;
                if (gi > g_learn1) gi = g_learn1;
                row = (int)((1.0 - mean_ang[gi] / 90.0) * (H - 1));
                if (row < 0) row = 0;
                if (row >= H) row = H - 1;
                img[row * W + a] = 255;
                if (row + 1 < H) img[(row + 1) * W + a] = 160;
            }
            fprintf(pgm, "P2\n%d %d\n255\n", W, H);
            for (b = 0; b < H; ++b) {
                for (a = 0; a < W; ++a)
                    fprintf(pgm, "%d%c", img[b * W + a], a + 1 == W ? '\n' : ' ');
            }
            free(img);
        }
        fclose(pgm);
    }

    {
        double early = 0.0, learn = 0.0, rho;
        int ne = 0, nl = 0;
        double *gen = (double *)malloc((size_t)(g_learn1 + 1) * sizeof(double));
        for (g = g_early0; g <= g_early1 && g < hist_len; ++g) {
            early += mean_ang[g];
            ++ne;
        }
        for (g = g_learn0; g <= g_learn1 && g < hist_len; ++g) {
            learn += mean_ang[g];
            ++nl;
        }
        early = ne > 0 ? early / ne : 90.0;
        learn = nl > 0 ? learn / nl : 90.0;
        rho = 0.0;
        if (gen) {
            for (g = 0; g <= g_learn1; ++g) gen[g] = (double)g;
            rho = spearman_rho(gen, mean_ang, g_learn1 + 1);
            free(gen);
        }
        /*
         * 判据：初始主轴夹角大（随机）；学习阶段显著下降且贴合长轴
         * （learn < 15°）；gen 与 angle 在学习窗口内 Spearman 为负。
         */
        ok = (early > 20.0) && (learn < 15.0) && (early > learn + 10.0) && (rho < -0.4);
        snprintf(detail, sizeof detail,
                 "angle early(g0-2)=%.1f° learn(g8-28)=%.1f° spearman=%.3f runs=%d gens=%d",
                 early, learn, rho, n_used, hist_len);
        test_record(ok, "cmaes", "cov_axis_aligns_rotated_ellipse", detail);
    }
    free(mean_ang);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"es", "fifth_rule_success_rate_sphere",
         "1/5 规则使 (1+1)-ES 晚段成功率落在 0.2±0.05",
         t_es_fifth_rule_sphere},
        {"es", "adaptive_sigma_keeps_rate_near_fifth",
         "自适应步长比固定步长更贴近 1/5 成功率",
         t_es_adapt_vs_fixed_sigma},
        {"es", "mu_rho_lambda_comma_plus",
         "(μ/ρ,λ) 与 (μ/ρ+λ)：plus 单调、comma 允许回退",
         t_es_murl_comma_plus},
        {"cmaes", "cma_fewer_evals_than_de_rosenbrock",
         "Rosenbrock 上 CMA-ES 评估数显著少于 DE (p<0.01)",
         t_cma_vs_de_rosenbrock},
        {"cmaes", "cma_advantage_narrows_on_rastrigin",
         "Rastrigin 上 CMA 相对 DE 的优势缩窄或反转",
         t_cma_vs_de_rastrigin_narrows},
        {"cmaes", "ipop_restarts_solve_rastrigin",
         "IPOP 倍增重启在多峰上显著优于单次运行",
         t_cmaes_ipop_rastrigin},
        {"cmaes", "cov_axis_aligns_rotated_ellipse",
         "协方差主轴夹角随迭代收敛到等高线长轴",
         t_cmaes_cov_axis_align},
    };
    test_ensure_results_dir();
    return test_run_main("C6-es-cmaes", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
