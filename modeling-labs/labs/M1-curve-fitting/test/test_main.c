/*
 * M1: 曲线拟合工作台 — 双引擎 + CI 覆盖率 + σ 线性放大 + 自动报告
 */
#include "harness.h"
#include "lab.h"
#include "fitbench.h"
#include "lsq.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double loglog_slope(const double *x, const double *y, int n)
{
    double sx = 0, sy = 0, sxx = 0, sxy = 0, den;
    int i;
    if (n < 2) return 0.0;
    for (i = 0; i < n; ++i) {
        double lx = log(x[i]), ly = log(y[i]);
        sx += lx; sy += ly; sxx += lx * lx; sxy += lx * ly;
    }
    den = (double)n * sxx - sx * sx;
    if (fabs(den) < 1e-300) return 0.0;
    return ((double)n * sxy - sx * sy) / den;
}

/* 设计：t 均匀 [0,4]，模型 a exp(-b t) */
static void fill_t_grid(double *t, int m, double a, double b)
{
    int i;
    for (i = 0; i < m; ++i)
        t[i] = a + (b - a) * (double)i / (double)(m - 1);
}

static int near_true(const double *th, const double *true_th, int npar, double rtol)
{
    int j;
    for (j = 0; j < npar; ++j) {
        double scale = fmax(1.0, fabs(true_th[j]));
        if (fabs(th[j] - true_th[j]) > rtol * scale) return 0;
    }
    return 1;
}

/* ---------- suite: pipeline ---------- */

static int t_pipeline_dual_engine_demo(void)
{
    /* 流水线样例：合成 → LM/BFGS → 报告落盘 */
    const int m = 40;
    const double th_true[2] = {1.8, 0.65};
    const double sigma = 0.05;
    double t[40];
    double th_lm[2], th_bf[2];
    mlab_fit_data d;
    mlab_fit_result rlm, rbf;
    int ok;
    char detail[220];

    fill_t_grid(t, m, 0.0, 4.0);
    memset(&d, 0, sizeof d);
    d.model_id = MLAB_FIT_MODEL_EXP;
    d.npar = 2;
    d.theta_true = th_true;
    d.sigma = sigma;
    d.m = m;
    d.t = t;
    if (mlab_fit_synth(&d, 90101) != 0 ||
        mlab_fit_result_alloc(&rlm, 2) != 0 ||
        mlab_fit_result_alloc(&rbf, 2) != 0) {
        free(d.y); free(d.y_clean);
        test_record(0, "pipeline", "dual_engine_synthetic_demo", "oom");
        return 0;
    }
    th_lm[0] = th_true[0] * 1.1;
    th_lm[1] = th_true[1] * 0.9;
    memcpy(th_bf, th_lm, sizeof th_lm);
    mlab_fit_lm(t, d.y, m, MLAB_FIT_MODEL_EXP, th_lm, 2, 200, 1e-12, &rlm);
    mlab_fit_bfgs(t, d.y, m, MLAB_FIT_MODEL_EXP, th_bf, 2, 200, 1e-10, &rbf);

    test_ensure_results_dir();
    mlab_fit_write_report("results/m1_demo_lm.md", "M1 demo LM", &d, &rlm, 0);
    mlab_fit_write_report("results/m1_demo_bfgs.md", "M1 demo BFGS", &d, &rbf, 0);

    ok = (rlm.status == 0) && (rbf.status == 0) &&
         near_true(rlm.theta, th_true, 2, 0.15) &&
         near_true(rbf.theta, th_true, 2, 0.15);
    snprintf(detail, sizeof detail,
             "LM th=(%.4f,%.4f) RMSE=%.4f | BFGS th=(%.4f,%.4f) RMSE=%.4f | true=(%.2f,%.2f)",
             rlm.theta[0], rlm.theta[1], rlm.rmse,
             rbf.theta[0], rbf.theta[1], rbf.rmse,
             th_true[0], th_true[1]);
    test_record(ok, "pipeline", "dual_engine_synthetic_demo", detail);
    mlab_fit_result_free(&rlm);
    mlab_fit_result_free(&rbf);
    free(d.y);
    free(d.y_clean);
    return ok;
}

/* ---------- suite: coverage ---------- */

static int t_coverage_500_repeats(void)
{
    /*
     * 判据：500 次重复实验 95% CI 覆盖率 ∈ [88%, 99%]。
     * 口径：CI 用 t(m-npar) 分位（此处 t(28)），非正态 1.96；
     *       每参数边际覆盖率，以及「两参数同时落入」的联合率记入报告。
     */
    const int n_rep = 500;
    const int m = 30;
    const double th_true[2] = {2.0, 0.8};
    const double sigma = 0.08;
    double t[30];
    int hit0 = 0, hit1 = 0, hit_all = 0;
    int r, ok;
    double c0, c1, ca;
    char detail[240];
    FILE *fp;
    int fail_fit = 0;

    fill_t_grid(t, m, 0.0, 3.5);
    test_ensure_results_dir();
    fp = fopen("results/m1_coverage_trials.csv", "w");
    if (fp) fprintf(fp, "rep,th0,se0,hit0,th1,se1,hit1,hit_all\n");

    for (r = 0; r < n_rep; ++r) {
        mlab_fit_data d;
        mlab_fit_result res;
        double th0[2];
        int hits[2];
        int all;

        memset(&d, 0, sizeof d);
        d.model_id = MLAB_FIT_MODEL_EXP;
        d.npar = 2;
        d.theta_true = th_true;
        d.sigma = sigma;
        d.m = m;
        d.t = t;
        if (mlab_fit_synth(&d, 910000u + (unsigned)r * 13u) != 0) {
            free(d.y); free(d.y_clean);
            ++fail_fit;
            continue;
        }
        if (mlab_fit_result_alloc(&res, 2) != 0) {
            free(d.y); free(d.y_clean);
            ++fail_fit;
            continue;
        }
        th0[0] = th_true[0] * 1.05;
        th0[1] = th_true[1] * 0.95;
        /* tol 1e-10：FD 雅可比下 1e-12 的相对 RSS 改善无法可靠触发，会误入停滞分支 */
        if (mlab_fit_lm(t, d.y, m, MLAB_FIT_MODEL_EXP, th0, 2, 200, 1e-10, &res) != 0) {
            ++fail_fit;
            if (fp) fprintf(fp, "%d,,,,,,,,\n", r);
            mlab_fit_result_free(&res);
            free(d.y); free(d.y_clean);
            continue;
        }
        all = mlab_fit_ci95_contains(&res, th_true, m - res.npar, hits);
        if (hits[0]) ++hit0;
        if (hits[1]) ++hit1;
        if (all) ++hit_all;
        if (fp)
            fprintf(fp, "%d,%.6f,%.6f,%d,%.6f,%.6f,%d,%d\n",
                    r, res.theta[0], res.se[0], hits[0],
                    res.theta[1], res.se[1], hits[1], all);
        mlab_fit_result_free(&res);
        free(d.y);
        free(d.y_clean);
    }
    if (fp) fclose(fp);

    c0 = (double)hit0 / (double)n_rep;
    c1 = (double)hit1 / (double)n_rep;
    ca = (double)hit_all / (double)n_rep;
    /* 边际覆盖率须落在 [0.88, 0.99] */
    ok = (c0 >= 0.88) && (c0 <= 0.99) && (c1 >= 0.88) && (c1 <= 0.99) &&
         (fail_fit == 0);
    snprintf(detail, sizeof detail,
             "500 reps LM | coverage th0=%.1f%% th1=%.1f%% joint=%.1f%% | band [88,99]%% | fail_fit=%d",
             100 * c0, 100 * c1, 100 * ca, fail_fit);
    test_record(ok, "coverage", "ci95_coverage_500_repeats", detail);
    return ok;
}

/* ---------- suite: domain ---------- */

static int t_domain_lm_bfgs_success_rate(void)
{
    /*
     * 判据：LM 与 BFGS 在各自收敛域内成功率 100%。
     * 收敛域：初值在真值 ±20% 邻域。
     * 成功口径（双口径）：引擎收敛（status==0）**且** 参数恢复（相对误差 < 10%）。
     */
    const int n_rep = 30;
    const int m = 30;
    const double th_true[2] = {1.5, 0.7};
    const double sigma = 0.04;
    double t[30];
    int ok_lm = 0, ok_bf = 0, conv_lm = 0, conv_bf = 0;
    int r;
    char detail[220];
    FILE *fp;

    fill_t_grid(t, m, 0.0, 3.0);
    test_ensure_results_dir();
    fp = fopen("results/m1_domain_success.csv", "w");
    if (fp) fprintf(fp, "rep,lm_conv,lm_rec,lm_ok,bfgs_conv,bfgs_rec,bfgs_ok,rss_lm,rss_bfgs\n");

    for (r = 0; r < n_rep; ++r) {
        mlab_fit_data d;
        mlab_fit_result rlm, rbf;
        double th_lm[2], th_bf[2];
        int lm_conv, lm_rec, lm_ok, bf_conv, bf_rec, bf_ok;
        mlab_rng rng;

        memset(&d, 0, sizeof d);
        d.model_id = MLAB_FIT_MODEL_EXP;
        d.npar = 2;
        d.theta_true = th_true;
        d.sigma = sigma;
        d.m = m;
        d.t = t;
        if (mlab_fit_synth(&d, 920000u + (unsigned)r * 7u) != 0) {
            free(d.y); free(d.y_clean);
            continue;
        }
        if (mlab_fit_result_alloc(&rlm, 2) != 0 ||
            mlab_fit_result_alloc(&rbf, 2) != 0) {
            mlab_fit_result_free(&rlm);
            free(d.y); free(d.y_clean);
            continue;
        }
        mlab_rng_seed(&rng, 930000u + (unsigned)r);
        /* 真值 ±20% 初值扰动（均匀落在 [0.8, 1.2]× 真值） */
        th_lm[0] = th_true[0] * (0.8 + 0.4 * mlab_rng_uniform(&rng));
        th_lm[1] = th_true[1] * (0.8 + 0.4 * mlab_rng_uniform(&rng));
        memcpy(th_bf, th_lm, sizeof th_lm);
        lm_conv = mlab_fit_lm(t, d.y, m, MLAB_FIT_MODEL_EXP, th_lm, 2, 300, 1e-10, &rlm) == 0;
        lm_rec = near_true(rlm.theta, th_true, 2, 0.10);
        lm_ok = lm_conv && lm_rec;
        /* BFGS tol 1e-8：f≈0.024 时梯度范数的双精度可达下限约 √(ε·f·λ)≈1e-8，再紧线搜索必然失败 */
        bf_conv = mlab_fit_bfgs(t, d.y, m, MLAB_FIT_MODEL_EXP, th_bf, 2, 800, 1e-8, &rbf) == 0;
        bf_rec = near_true(rbf.theta, th_true, 2, 0.10);
        bf_ok = bf_conv && bf_rec;
        if (lm_ok) ++ok_lm;
        if (lm_conv) ++conv_lm;
        if (bf_ok) ++ok_bf;
        if (bf_conv) ++conv_bf;
        if (fp)
            fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,%.6g,%.6g\n",
                    r, lm_conv, lm_rec, lm_ok, bf_conv, bf_rec, bf_ok, rlm.rss, rbf.rss);
        mlab_fit_result_free(&rlm);
        mlab_fit_result_free(&rbf);
        free(d.y);
        free(d.y_clean);
    }
    if (fp) fclose(fp);
    snprintf(detail, sizeof detail,
             "domain ±20%% × %d | LM 收敛 %d/%d，恢复+收敛（双口径）%d/%d | "
             "BFGS 收敛 %d/%d，恢复+收敛 %d/%d (need 100%%)",
             n_rep, conv_lm, n_rep, ok_lm, n_rep, conv_bf, n_rep, ok_bf, n_rep);
    test_record(ok_lm == n_rep && ok_bf == n_rep, "domain", "lm_bfgs_success_in_domain",
                detail);
    return ok_lm == n_rep && ok_bf == n_rep;
}

/* ---------- suite: noise scaling ---------- */

static int t_noise_sigma_linear_scaling(void)
{
    /*
     * 判据：5 类噪声水平下参数恢复误差随 σ 线性放大（斜率 ≈ 理论值）。
     * 理论：std(θ̂) ≈ σ * sqrt((JᵀJ)^{-1}_{jj})；mean|err| ≈ 0.8 σ * se/σ。
     * 验收：log-log 斜率 ≈ 1（|slope-1|<0.25）；且 mean|err|/σ 与理论 se/σ 同量级。
     */
    const int nsig = 5;
    const double sigmas[5] = {0.02, 0.04, 0.08, 0.16, 0.32};
    const int n_rep = 40;
    const int m = 40;
    const double th_true[2] = {1.2, 0.5};
    double t[40];
    double mean_err_b[5], mean_err_a[5];
    double slope_b, slope_a;
    int s, r, ok;
    char detail[260];
    FILE *fp;

    fill_t_grid(t, m, 0.0, 4.0);
    test_ensure_results_dir();
    fp = fopen("results/m1_sigma_scan.csv", "w");
    if (fp) fprintf(fp, "sigma,mean_abs_err_a,mean_abs_err_b,theo_se_a,theo_se_b,n_rep\n");

    for (s = 0; s < nsig; ++s) {
        double sum_ea = 0.0, sum_eb = 0.0;
        double theo_a = 0.0, theo_b = 0.0;
        int n_ok = 0;
        for (r = 0; r < n_rep; ++r) {
            mlab_fit_data d;
            mlab_fit_result res;
            double th0[2];
            memset(&d, 0, sizeof d);
            d.model_id = MLAB_FIT_MODEL_EXP;
            d.npar = 2;
            d.theta_true = th_true;
            d.sigma = sigmas[s];
            d.m = m;
            d.t = t;
            if (mlab_fit_synth(&d, 940000u + (unsigned)(s * 1000 + r) * 3u) != 0) {
                free(d.y); free(d.y_clean);
                continue;
            }
            if (mlab_fit_result_alloc(&res, 2) != 0) {
                free(d.y); free(d.y_clean);
                continue;
            }
            th0[0] = th_true[0] * 1.05;
            th0[1] = th_true[1] * 0.95;
            if (mlab_fit_lm(t, d.y, m, MLAB_FIT_MODEL_EXP, th0, 2, 200, 1e-12, &res) == 0) {
                sum_ea += fabs(res.theta[0] - th_true[0]);
                sum_eb += fabs(res.theta[1] - th_true[1]);
                theo_a += res.se[0];
                theo_b += res.se[1];
                ++n_ok;
            }
            mlab_fit_result_free(&res);
            free(d.y);
            free(d.y_clean);
        }
        mean_err_a[s] = n_ok ? sum_ea / n_ok : 0.0;
        mean_err_b[s] = n_ok ? sum_eb / n_ok : 0.0;
        if (fp)
            fprintf(fp, "%.4f,%.6g,%.6g,%.6g,%.6g,%d\n",
                    sigmas[s], mean_err_a[s], mean_err_b[s],
                    n_ok ? theo_a / n_ok : 0.0,
                    n_ok ? theo_b / n_ok : 0.0, n_ok);
    }
    if (fp) fclose(fp);

    slope_b = fabs(loglog_slope(sigmas, mean_err_b, nsig));
    slope_a = fabs(loglog_slope(sigmas, mean_err_a, nsig));
    /*
     * 理论斜率 ≈ 1（误差 ∝ σ）。
     * 同时检查最大 σ 与最小 σ 的误差比 ≈ σ 比（16x）。
     */
    ok = (slope_b >= 0.75) && (slope_b <= 1.25) &&
         (slope_a >= 0.75) && (slope_a <= 1.25) &&
         (mean_err_b[nsig - 1] > 5.0 * mean_err_b[0]);
    snprintf(detail, sizeof detail,
             "σ∈[0.02,0.32]×%d | loglog slope a=%.3f b=%.3f (≈1±0.25) | "
             "err_b(0.32)/err_b(0.02)=%.2f",
             n_rep, slope_a, slope_b,
             mean_err_b[0] > 0 ? mean_err_b[nsig - 1] / mean_err_b[0] : 0.0);
    test_record(ok, "noise", "recovery_error_scales_linearly_with_sigma", detail);
    return ok;
}

static int t_noise_theory_ratio(void)
{
    /* 额外：mean|err| / (σ * theoretical se/σ) 与高斯 0.8 同量级 */
    const int m = 40;
    const double th_true[2] = {1.2, 0.5};
    const double sigma = 0.1;
    const int n_rep = 80;
    double t[40];
    double sum_err = 0.0, sum_se = 0.0;
    int r, n_ok = 0, ok;
    double ratio;
    char detail[180];

    fill_t_grid(t, m, 0.0, 4.0);
    for (r = 0; r < n_rep; ++r) {
        mlab_fit_data d;
        mlab_fit_result res;
        double th0[2] = {th_true[0] * 1.05, th_true[1] * 0.95};
        memset(&d, 0, sizeof d);
        d.model_id = MLAB_FIT_MODEL_EXP;
        d.npar = 2;
        d.theta_true = th_true;
        d.sigma = sigma;
        d.m = m;
        d.t = t;
        if (mlab_fit_synth(&d, 950000u + (unsigned)r) != 0) {
            free(d.y); free(d.y_clean);
            continue;
        }
        if (mlab_fit_result_alloc(&res, 2) != 0) {
            free(d.y); free(d.y_clean);
            continue;
        }
        if (mlab_fit_lm(t, d.y, m, MLAB_FIT_MODEL_EXP, th0, 2, 200, 1e-12, &res) == 0) {
            sum_err += fabs(res.theta[1] - th_true[1]);
            sum_se += res.se[1];
            ++n_ok;
        }
        mlab_fit_result_free(&res);
        free(d.y);
        free(d.y_clean);
    }
    ratio = (sum_se > 0) ? (sum_err / n_ok) / (sum_se / n_ok) : 0.0;
    /* E|N(0,s)| = s*sqrt(2/π) ≈ 0.798 s */
    ok = (n_ok >= 50) && (ratio > 0.5) && (ratio < 1.2);
    snprintf(detail, sizeof detail,
             "mean|err|/mean(se) on θ_b = %.3f (theory≈0.80); n_ok=%d", ratio, n_ok);
    test_record(ok, "noise", "error_to_se_ratio_near_gaussian", detail);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"pipeline", "dual_engine_synthetic_demo",
         "合成数据双引擎拟合 + 报告落盘",
         t_pipeline_dual_engine_demo},
        {"coverage", "ci95_coverage_500_repeats",
         "500 次重复 95% CI 覆盖率 ∈ [88%, 99%]（t(28) 分位）",
         t_coverage_500_repeats},
        {"domain", "lm_bfgs_success_in_domain",
         "收敛域（真值 ±20% 初值）内 LM/BFGS 成功率 100%（收敛+恢复双口径）",
         t_domain_lm_bfgs_success_rate},
        {"noise", "recovery_error_scales_linearly_with_sigma",
         "5 档 σ 下恢复误差线性放大（斜率≈1）",
         t_noise_sigma_linear_scaling},
        {"noise", "error_to_se_ratio_near_gaussian",
         "mean|err|/se ≈ 高斯常数 0.8",
         t_noise_theory_ratio},
    };
    test_ensure_results_dir();
    return test_run_main("M1-curve-fitting", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
