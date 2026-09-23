/*
 * C2: MH / Gibbs / 链诊断（IAT、Gelman-Rubin R̂）
 * 判据:
 *  - 2D 高斯: |mean err| < 3 SE；协方差元素相对误差 < 10%
 *  - Gibbs IAT / RWM-MH IAT < 1/3（同目标，ρ=0.95）
 *  - burn-in 后 R̂ < 1.1 且持续（4 链）
 */
#include "harness.h"
#include "lab.h"
#include "mcmc.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

static double log_pi_diag_gauss(const double *x, int d, void *ctx)
{
    const double *p = ctx; /* mu0,s0,mu1,s1 */
    double z0 = (x[0] - p[0]) / p[1];
    double z1 = (d > 1) ? (x[1] - p[2]) / p[3] : 0.0;
    return -0.5 * (z0 * z0 + z1 * z1);
}

static double se_mean(double sigma, double tau, int n)
{
    if (n <= 0) return 1e9;
    return sigma * sqrt(tau / (double)n);
}

static void write_csv_header(FILE *fp, const char *hdr)
{
    if (!fp) return;
    fprintf(fp, "%s\n", hdr);
}

/* ---------- suite: mh ---------- */

static int t_mh_diag_gaussian_moments(void)
{
    /* E1: RWM 采样对角 2D 高斯 N([1,-0.5], diag(1.5², 0.8²)) */
    const double mu[2] = {1.0, -0.5};
    const double sig[2] = {1.5, 0.8};
    double ctx[4] = {mu[0], sig[0], mu[1], sig[1]};
    double x0[2] = {0.0, 0.0};
    const int n_burn = 2000, n_keep = 40000, thin = 1;
    double step = 1.2;
    mlab_rng rng;
    mlab_chain ch;
    double mean[2], cov[4], tau[2], se[2];
    double e0, e1, c00, c11, c01;
    int ok_mean, ok_cov, ok;
    FILE *fp;
    char detail[192];

    mlab_rng_seed_kind(&rng, 6201, MLAB_RNG_SPLITMIX64);
    if (!mlab_mh_rwm(&rng, log_pi_diag_gauss, ctx, x0, 2, step,
                     n_burn, n_keep, thin, &ch)) {
        test_record(0, "mh", "diag_gaussian_moments", "alloc/run fail");
        return 0;
    }
    mlab_chain_mean(&ch, mean);
    mlab_chain_cov(&ch, cov);
    tau[0] = mlab_chain_iat(&ch, 0, 400);
    tau[1] = mlab_chain_iat(&ch, 1, 400);
    se[0] = se_mean(sig[0], tau[0], n_keep);
    se[1] = se_mean(sig[1], tau[1], n_keep);
    e0 = fabs(mean[0] - mu[0]);
    e1 = fabs(mean[1] - mu[1]);
    c00 = cov[0];
    c11 = cov[3];
    c01 = cov[1];
    /* 协方差元素相对误差（对角用 σ²，非对角用 σ1σ2 作尺度） */
    {
        double r00 = fabs(c00 - sig[0] * sig[0]) / (sig[0] * sig[0]);
        double r11 = fabs(c11 - sig[1] * sig[1]) / (sig[1] * sig[1]);
        double r01 = fabs(c01 - 0.0) / (sig[0] * sig[1]);
        ok_mean = e0 < 3.0 * se[0] && e1 < 3.0 * se[1];
        ok_cov = r00 < 0.10 && r11 < 0.10 && r01 < 0.10;
        ok = ok_mean && ok_cov && ch.accept_rate > 0.15 && ch.accept_rate < 0.70;
        snprintf(detail, sizeof detail,
                 "m0=%.4f±%.4f e0=%.4f m1=%.4f±%.4f e1=%.4f | "
                 "cov11=%.4f r=%.3f cov22=%.4f r=%.3f cov12=%.4f r=%.3f | acc=%.3f tau=%.1f/%.1f",
                 mean[0], se[0], e0, mean[1], se[1], e1,
                 c00, r00, c11, r11, c01, r01,
                 ch.accept_rate, tau[0], tau[1]);
        test_record(ok, "mh", "diag_gaussian_moments", detail);
    }
    fp = fopen("results/mh_gaussian.csv", "w");
    if (fp) {
        write_csv_header(fp, "algo,mean0,mean1,cov00,cov01,cov11,acc,iat0,iat1,n_keep");
        fprintf(fp, "rwm,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.3f,%.3f,%d\n",
                mean[0], mean[1], c00, c01, c11, ch.accept_rate, tau[0], tau[1], n_keep);
        fclose(fp);
    }
    mlab_chain_free(&ch);
    return ok;
}

static int t_mh_correlated_gaussian_rw(void)
{
    /* 强相关 ρ=0.95 的 RWM：矩对账 + 接受率合理 */
    const double mu[2] = {0.0, 0.0};
    const double s1 = 1.0, s2 = 1.0, rho = 0.95;
    mlab_bvn_ctx ctx;
    double x0[2] = {-2.0, 2.0};
    const int n_burn = 5000, n_keep = 50000;
    /* 沿短轴（特征值 1-ρ）调步，避免完全拒绝 */
    double step = 0.35;
    mlab_rng rng;
    mlab_chain ch;
    double mean[2], cov[4], tau0;
    double c12_true = rho * s1 * s2;
    double se0;
    int ok;
    char detail[160];
    FILE *fp;

    ctx.mu[0] = mu[0];
    ctx.mu[1] = mu[1];
    ctx.s1 = s1;
    ctx.s2 = s2;
    ctx.rho = rho;
    mlab_rng_seed_kind(&rng, 6202, MLAB_RNG_SPLITMIX64);
    if (!mlab_mh_rwm(&rng, mlab_bvn_logpdf, &ctx, x0, 2, step,
                     n_burn, n_keep, 1, &ch)) {
        test_record(0, "mh", "correlated_gaussian_rw", "run fail");
        return 0;
    }
    mlab_chain_mean(&ch, mean);
    mlab_chain_cov(&ch, cov);
    tau0 = mlab_chain_iat(&ch, 0, 800);
    se0 = se_mean(s1, tau0, n_keep);
    ok = fabs(mean[0]) < 3.0 * se0 && fabs(mean[1]) < 3.0 * se0
         && fabs(cov[1] - c12_true) / fabs(c12_true) < 0.10
         && fabs(cov[0] - 1.0) < 0.15
         && ch.accept_rate > 0.05 && ch.accept_rate < 0.60;
    snprintf(detail, sizeof detail,
             "mean=(%.4f,%.4f) cov12=%.4f (true %.4f) acc=%.3f iat0=%.1f se0=%.4f",
             mean[0], mean[1], cov[1], c12_true, ch.accept_rate, tau0, se0);
    test_record(ok, "mh", "correlated_gaussian_rw", detail);
    fp = fopen("results/mh_gaussian.csv", "a");
    if (fp) {
        fprintf(fp, "rwm_corr,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.3f,%.3f,%d\n",
                mean[0], mean[1], cov[0], cov[1], cov[3], ch.accept_rate,
                tau0, mlab_chain_iat(&ch, 1, 800), n_keep);
        fclose(fp);
    }
    mlab_chain_free(&ch);
    return ok;
}

/* ---------- suite: gibbs ---------- */

static int t_gibbs_conjugate_analytical(void)
{
    /* E3 前半：共轭正态后验，Gibbs 矩 vs 解析后验 */
    const double mu0[2] = {0.0, 0.0};
    const double s0 = 3.0;
    const double s1 = 1.0, s2 = 1.0, rho = 0.8;
    const double mu_true[2] = {1.2, -0.4};
    const int n_data = 40;
    double *data;
    mlab_bvn_posterior post;
    mlab_rng rng;
    mlab_chain ch;
    double x0[2] = {0.0, 0.0};
    const int n_burn = 2000, n_keep = 30000;
    double mean[2], cov[4], tau0, se0;
    int ok, i;
    char detail[176];
    FILE *fp;

    data = (double *)malloc(sizeof(double) * (size_t)n_data * 2);
    if (!data) {
        test_record(0, "gibbs", "conjugate_bvn_analytical", "oom");
        return 0;
    }
    mlab_rng_seed_kind(&rng, 6203, MLAB_RNG_SPLITMIX64);
    /* 数据 ~ N(mu_true, Σ) */
    for (i = 0; i < n_data; ++i) {
        double z1 = mlab_rng_normal(&rng);
        double z2 = mlab_rng_normal(&rng);
        data[2 * i + 0] = mu_true[0] + s1 * z1;
        data[2 * i + 1] = mu_true[1] + s2 * (rho * z1 + sqrt(1 - rho * rho) * z2);
    }
    memset(&post, 0, sizeof post);
    mlab_bvn_conjugate_posterior(mu0, s0, s1, s2, rho, data, n_data, 2, &post);
    if (post.s1 <= 0 || post.s2 <= 0) {
        free(data);
        test_record(0, "gibbs", "conjugate_bvn_analytical", "posterior fail");
        return 0;
    }
    /* 后验应向真值收缩 */
    if (!mlab_gibbs_bvn(&rng, post.mu, post.s1, post.s2, post.rho, x0,
                        n_burn, n_keep, 1, &ch)) {
        free(data);
        test_record(0, "gibbs", "conjugate_bvn_analytical", "gibbs fail");
        return 0;
    }
    mlab_chain_mean(&ch, mean);
    mlab_chain_cov(&ch, cov);
    tau0 = mlab_chain_iat(&ch, 0, 400);
    se0 = se_mean(post.s1, tau0, n_keep);
    ok = fabs(mean[0] - post.mu[0]) < 3.0 * se0
         && fabs(mean[1] - post.mu[1]) < 3.0 * se0
         && fabs(cov[0] - post.s1 * post.s1) / (post.s1 * post.s1) < 0.10
         && fabs(cov[3] - post.s2 * post.s2) / (post.s2 * post.s2) < 0.10
         && fabs(cov[1] - post.rho * post.s1 * post.s2)
                / (post.s1 * post.s2) < 0.10
         && fabs(post.mu[0] - mu_true[0]) < 3.0 * post.s1 + 0.5
         && fabs(post.mu[1] - mu_true[1]) < 3.0 * post.s2 + 0.5;
    snprintf(detail, sizeof detail,
             "post_mu=(%.4f,%.4f) post_s=(%.4f,%.4f) rho=%.4f | "
             "gibbs_mean=(%.4f,%.4f) se=%.4f | "
             "cov=(%.5f,%.5f,%.5f) want12=%.5f",
             post.mu[0], post.mu[1], post.s1, post.s2, post.rho,
             mean[0], mean[1], se0,
             cov[0], cov[3], cov[1],
             post.rho * post.s1 * post.s2);
    test_record(ok, "gibbs", "conjugate_bvn_analytical", detail);
    fp = fopen("results/gibbs_mh_iat.csv", "w");
    if (fp) {
        write_csv_header(fp, "case,algo,mean0,mean1,cov00,cov11,cov12,iat0,iat1,acc,n_keep");
        fprintf(fp, "conjugate,gibbs,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%d\n",
                mean[0], mean[1], cov[0], cov[3], cov[1],
                tau0, mlab_chain_iat(&ch, 1, 400), ch.accept_rate, n_keep);
        fprintf(fp, "conjugate,analytical,%.6f,%.6f,%.6f,%.6f,%.6f,,,,\n",
                post.mu[0], post.mu[1], post.s1 * post.s1, post.s2 * post.s2,
                post.rho * post.s1 * post.s2);
        fclose(fp);
    }
    mlab_chain_free(&ch);
    free(data);
    return ok;
}

static int t_gibbs_vs_mh_iat_ratio(void)
{
    /* E2/E3: 同目标 ρ=0.95，Gibbs IAT 显著短于 RWM-MH，比值 < 1/3 */
    const double mu[2] = {0.5, -0.2};
    const double s1 = 1.0, s2 = 1.0, rho = 0.95;
    mlab_bvn_ctx ctx;
    mlab_rng rng_g, rng_m;
    mlab_chain ch_g, ch_m;
    double x0[2] = {3.0, -3.0};
    const int n_burn = 4000, n_keep = 60000;
    double step_mh = 0.25; /* 未能沿相关脊，IAT 会很大 */
    double iat_g, iat_m, ratio;
    double mg[2], mm[2];
    int ok;
    char detail[160];
    FILE *fp;

    ctx.mu[0] = mu[0];
    ctx.mu[1] = mu[1];
    ctx.s1 = s1;
    ctx.s2 = s2;
    ctx.rho = rho;

    mlab_rng_seed_kind(&rng_g, 6210, MLAB_RNG_SPLITMIX64);
    mlab_rng_seed_kind(&rng_m, 6211, MLAB_RNG_SPLITMIX64);
    if (!mlab_gibbs_bvn(&rng_g, mu, s1, s2, rho, x0, n_burn, n_keep, 1, &ch_g) ||
        !mlab_mh_rwm(&rng_m, mlab_bvn_logpdf, &ctx, x0, 2, step_mh,
                     n_burn, n_keep, 1, &ch_m)) {
        test_record(0, "gibbs", "vs_mh_iat_ratio", "run fail");
        return 0;
    }
    mlab_chain_mean(&ch_g, mg);
    mlab_chain_mean(&ch_m, mm);
    iat_g = mlab_chain_iat(&ch_g, 0, 800);
    iat_m = mlab_chain_iat(&ch_m, 0, 800);
    ratio = iat_g / iat_m;
    /* Gibbs 矩也应接近真值 */
    ok = ratio < 1.0 / 3.0
         && iat_g > 1.0
         && fabs(mg[0] - mu[0]) < 0.08
         && fabs(mg[1] - mu[1]) < 0.08
         && ch_m.accept_rate > 0.02; /* 链确实在动，不是卡死 */
    snprintf(detail, sizeof detail,
             "iat_gibbs=%.2f iat_mh=%.2f ratio=%.4f (need<0.333) "
             "mean_g=(%.3f,%.3f) mean_m=(%.3f,%.3f) acc_mh=%.3f",
             iat_g, iat_m, ratio, mg[0], mg[1], mm[0], mm[1], ch_m.accept_rate);
    test_record(ok, "gibbs", "vs_mh_iat_ratio", detail);

    /* thin 必要性：RWM 链 IAT 大 → 有效样本远少于名义样本 */
    {
        double iat_m1 = mlab_chain_iat(&ch_m, 1, 800);
        double neff_m = n_keep / iat_m;
        double neff_g = n_keep / iat_g;
        FILE *fp2 = fopen("results/gibbs_mh_iat.csv", "a");
        if (fp2) {
            fprintf(fp2, "rho095,gibbs,%.6f,%.6f,,,,%.3f,%.3f,1.000,%d\n",
                    mg[0], mg[1], iat_g, mlab_chain_iat(&ch_g, 1, 800), n_keep);
            fprintf(fp2, "rho095,rwm_mh,%.6f,%.6f,,,,%.3f,%.3f,%.3f,%d\n",
                    mm[0], mm[1], iat_m, iat_m1, ch_m.accept_rate, n_keep);
            fprintf(fp2, "rho095,neff,,,,,,%.0f,%.0f,,gibbs_n_eff_vs_mh\n",
                    neff_g, neff_m);
            fclose(fp2);
        }
        (void)fp;
    }
    mlab_chain_free(&ch_g);
    mlab_chain_free(&ch_m);
    return ok;
}

static int t_mh_thin_reduces_stored_acf(void)
{
    /*
     * thin 必要性对照：同一 RWM 目标，thin=1 vs thin≈IAT
     * 存盘样本 lag-1 ACF 应显著下降（更接近独立）。
     */
    const double mu[2] = {0.0, 0.0};
    const double s1 = 1.0, s2 = 1.0, rho = 0.95;
    mlab_bvn_ctx ctx;
    mlab_rng rng;
    mlab_chain ch1, chT;
    double x0[2] = {0.0, 0.0};
    double step = 0.25;
    const int n_burn = 4000, n_store = 8000, thin_hi = 80;
    double a1, aT, iat1, iatT;
    int ok;
    char detail[144];
    FILE *fp;

    ctx.mu[0] = mu[0];
    ctx.mu[1] = mu[1];
    ctx.s1 = s1;
    ctx.s2 = s2;
    ctx.rho = rho;
    mlab_rng_seed_kind(&rng, 6220, MLAB_RNG_SPLITMIX64);
    if (!mlab_mh_rwm(&rng, mlab_bvn_logpdf, &ctx, x0, 2, step,
                     n_burn, n_store, 1, &ch1)) {
        test_record(0, "mh", "thin_reduces_stored_acf", "thin1 fail");
        return 0;
    }
    mlab_rng_seed_kind(&rng, 6221, MLAB_RNG_SPLITMIX64);
    if (!mlab_mh_rwm(&rng, mlab_bvn_logpdf, &ctx, x0, 2, step,
                     n_burn, n_store, thin_hi, &chT)) {
        mlab_chain_free(&ch1);
        test_record(0, "mh", "thin_reduces_stored_acf", "thinT fail");
        return 0;
    }
    a1 = mlab_chain_acf(&ch1, 0, 1);
    aT = mlab_chain_acf(&chT, 0, 1);
    iat1 = mlab_chain_iat(&ch1, 0, 400);
    iatT = mlab_chain_iat(&chT, 0, 400);
    ok = fabs(a1) > 0.3
         && fabs(aT) < 0.5 * fabs(a1)
         && iatT < 0.25 * iat1
         && iatT < 12.0;
    snprintf(detail, sizeof detail,
             "acf1_thin1=%.4f acf1_thin%d=%.4f iat %.1f→%.1f acc=%.3f",
             a1, thin_hi, aT, iat1, iatT, ch1.accept_rate);
    test_record(ok, "mh", "thin_reduces_stored_acf", detail);
    fp = fopen("results/thin_acf.csv", "w");
    if (fp) {
        write_csv_header(fp, "thin,acf_lag1,iat,n_store,acc");
        fprintf(fp, "1,%.6f,%.3f,%d,%.4f\n", a1, iat1, n_store, ch1.accept_rate);
        fprintf(fp, "%d,%.6f,%.3f,%d,%.4f\n", thin_hi, aT, iatT, n_store, chT.accept_rate);
        fclose(fp);
    }
    mlab_chain_free(&ch1);
    mlab_chain_free(&chT);
    return ok;
}

/* ---------- suite: diagnostics (R̂) ---------- */

static int t_gelman_rubin_4chain(void)
{
    /* E4: 4 链并行，burn-in 后 R̂ < 1.1 且持续 */
    const double mu[2] = {0.0, 0.0};
    const double s1 = 1.0, s2 = 0.7, rho = 0.6;
    mlab_bvn_ctx ctx;
    const int m = 4;
    const int n_burn = 800, n_check = 1200; /* 每次诊断用的样本数步进 */
    const int n_steps = 10;                 /* 诊断点数 */
    const int n_max = n_check * n_steps;
    double starts[4][2] = {{-4, 4}, {4, -4}, {0, 5}, {-5, 0}};
    mlab_chain chains[4];
    mlab_rng rng;
    int j, t, ok_after = 1, first_ok = -1;
    double rhat_end = 0.0, rhat_max_after = 0.0;
    FILE *fp;
    char detail[176];

    ctx.mu[0] = mu[0];
    ctx.mu[1] = mu[1];
    ctx.s1 = s1;
    ctx.s2 = s2;
    ctx.rho = rho;

    for (j = 0; j < m; ++j) {
        mlab_rng_seed_kind(&rng, 6300 + j, MLAB_RNG_SPLITMIX64);
        if (!mlab_mh_rwm(&rng, mlab_bvn_logpdf, &ctx, starts[j], 2, 0.5,
                         0, n_max, 1, &chains[j])) {
            int k;
            for (k = 0; k < j; ++k) mlab_chain_free(&chains[k]);
            test_record(0, "diagnostics", "gelman_rubin_4chain", "run fail");
            return 0;
        }
    }
    fp = fopen("results/rhat_trace.csv", "w");
    if (fp) write_csv_header(fp, "n_used,rhat_max,rhat0,rhat1,phase");
    for (t = 1; t <= n_steps; ++t) {
        int n_used = n_check * t;
        double rh[2], rmax;
        /* R̂ 定义在“已丢弃 burn-in 的后验样本”上：
           这里把前 n_burn 视为 burn-in，诊断从 n_burn 之后取样。
           链从 t=0 起跑无额外 burn 参数，故取用 x[n_burn .. n_used)。 */
        double *seg[4];
        mlab_chain tmp[4];
        int j2;
        if (n_used <= n_burn) n_used = n_burn + n_check;
        if (n_used > n_max) n_used = n_max;
        for (j2 = 0; j2 < m; ++j2) {
            memset(&tmp[j2], 0, sizeof tmp[j2]);
            tmp[j2].x = chains[j2].x + (size_t)n_burn * 2;
            tmp[j2].d = 2;
            tmp[j2].n_keep = n_used - n_burn;
            seg[j2] = tmp[j2].x;
        }
        rmax = mlab_gelman_rubin((const double *const *)seg, m,
                                 n_used - n_burn, 2, rh);
        if (fp)
            fprintf(fp, "%d,%.6f,%.6f,%.6f,%s\n",
                    n_used - n_burn, rmax, rh[0], rh[1],
                    (n_used - n_burn) >= n_check * 3 ? "post_burn" : "early");
        /* “burn-in 后”：丢弃约 3 个诊断窗之后要求持续 < 1.1 */
        if (t >= 4) {
            if (rmax >= 1.1) ok_after = 0;
            if (rmax > rhat_max_after) rhat_max_after = rmax;
            if (first_ok < 0 && rmax < 1.1) first_ok = t;
            rhat_end = rmax;
        }
    }
    if (fp) fclose(fp);
    snprintf(detail, sizeof detail,
             "Rhat_end=%.4f max_after=%.4f (need all <1.1 for t>=4) first_ok_t=%d",
             rhat_end, rhat_max_after, first_ok);
    test_record(ok_after && rhat_end < 1.1, "diagnostics", "gelman_rubin_4chain", detail);
    for (j = 0; j < m; ++j) mlab_chain_free(&chains[j]);
    return ok_after && rhat_end < 1.1;
}

static int t_iat_white_noise_reference(void)
{
    /* 对照：近似独立样本 IAT ≈ 1（独立 MH 提议=目标） */
    const double mu[2] = {0.2, -0.3};
    const double sig[2] = {1.0, 0.8};
    double ctx[4] = {mu[0], sig[0], mu[1], sig[1]};
    double prop_mu[2] = {0.2, -0.3};
    double x0[2] = {5.0, -5.0};
    mlab_rng rng;
    mlab_chain ch;
    double iat0, iat1, mean0;
    int ok;
    char detail[128];

    mlab_rng_seed_kind(&rng, 6400, MLAB_RNG_SPLITMIX64);
    /* 提议与目标相同 → 独立 MH 接受率≈1，链近似 iid */
    if (!mlab_mh_independent(&rng, log_pi_diag_gauss, ctx,
                             prop_mu, 1.0 /* 注意：提议各维 σ=1，目标 σ=(1,0.8) 略有失配 */,
                             x0, 2, 50, 20000, 1, &ch)) {
        test_record(0, "diagnostics", "iat_independent_mh_reference", "run fail");
        return 0;
    }
    /* 更干净的对照：提议精确匹配目标边距 */
    mlab_chain_free(&ch);
    mlab_rng_seed_kind(&rng, 6401, MLAB_RNG_SPLITMIX64);
    {
        /* 分维 σ 不同时用各维相同 σ 的提议不完全匹配；
           这里目标改为各维同 σ=1 的独立高斯，提议 N(mu, I) → 几乎独立。 */
        double ctx2[4] = {0.0, 1.0, 0.0, 1.0};
        double pm[2] = {0.0, 0.0};
        double x0b[2] = {2.0, -2.0};
        if (!mlab_mh_independent(&rng, log_pi_diag_gauss, ctx2,
                                 pm, 1.0, x0b, 2, 100, 20000, 1, &ch)) {
            test_record(0, "diagnostics", "iat_independent_mh_reference", "run2 fail");
            return 0;
        }
        iat0 = mlab_chain_iat(&ch, 0, 200);
        iat1 = mlab_chain_iat(&ch, 1, 200);
        mean0 = 0;
        {
            double m[2];
            mlab_chain_mean(&ch, m);
            mean0 = m[0];
        }
        ok = iat0 < 3.0 && iat1 < 3.0 && fabs(mean0) < 0.1;
        snprintf(detail, sizeof detail,
                 "iat=(%.2f,%.2f) mean0=%.4f acc=%.3f (need iat<3)",
                 iat0, iat1, mean0, ch.accept_rate);
        test_record(ok, "diagnostics", "iat_independent_mh_reference", detail);
        mlab_chain_free(&ch);
        return ok;
    }
}

/* ---------- suite: hmc ---------- */

/* d 维对角标准高斯：log π = -0.5·||x||²，∇log π = -x（HMC 对照用） */
static double log_pi_std_gauss(const double *x, int d, void *ctx)
{
    double s = 0.0;
    int i;
    (void)ctx;
    for (i = 0; i < d; ++i) s += x[i] * x[i];
    return -0.5 * s;
}

static void grad_std_gauss(const double *x, int d, void *ctx, double *g)
{
    int i;
    (void)ctx;
    for (i = 0; i < d; ++i) g[i] = -x[i];
}

/*
 * 高维对照（路线图 C2 HMC 注记）：d=16 对角高斯，同链长下
 * HMC 的 IAT 应显著短于 RWM；梯度引导提议的高维优势落到数据。
 */
static int t_hmc_beats_rwm_iat_highdim(void)
{
    const int d = 16;
    const int n_burn = 2000, n_keep = 20000;
    mlab_rng rng;
    mlab_chain ch_r, ch_h;
    double x0[16];
    double iat_r, iat_h, mh[16], hh[16], se_r, se_h;
    int j, ok;
    char detail[160];
    FILE *fp;

    for (j = 0; j < d; ++j) x0[j] = 0.0;
    mlab_rng_seed_kind(&rng, 6501, MLAB_RNG_SPLITMIX64);
    /* RWM 步长取经验最优 ~2.4/√d */
    if (!mlab_mh_rwm(&rng, log_pi_std_gauss, NULL, x0, d, 0.6,
                     n_burn, n_keep, 1, &ch_r)) {
        test_record(0, "hmc", "hmc_beats_rwm_iat_highdim", "rwm fail");
        return 0;
    }
    mlab_rng_seed_kind(&rng, 6502, MLAB_RNG_SPLITMIX64);
    if (!mlab_hmc(&rng, log_pi_std_gauss, grad_std_gauss, NULL, x0, d,
                  0.5, 20, n_burn, n_keep, 1, &ch_h)) {
        mlab_chain_free(&ch_r);
        test_record(0, "hmc", "hmc_beats_rwm_iat_highdim", "hmc fail");
        return 0;
    }
    iat_r = mlab_chain_iat(&ch_r, 0, 400);
    iat_h = mlab_chain_iat(&ch_h, 0, 400);
    mlab_chain_mean(&ch_r, mh);
    mlab_chain_mean(&ch_h, hh);
    se_r = se_mean(1.0, iat_r, n_keep);
    se_h = se_mean(1.0, iat_h, n_keep);
    ok = iat_h >= 1.0 && iat_h < 0.5 * iat_r
         && ch_h.accept_rate > 0.60 && ch_h.accept_rate <= 1.0
         && fabs(hh[0]) < 3.0 * se_h && fabs(mh[0]) < 3.0 * se_r;
    snprintf(detail, sizeof detail,
             "d=16 iat rwm=%.2f hmc=%.2f (ratio %.3f, need<0.5) "
             "acc_hmc=%.3f mean0 rwm=%.3f hmc=%.3f",
             iat_r, iat_h, iat_r > 0 ? iat_h / iat_r : 0,
             ch_h.accept_rate, mh[0], hh[0]);
    test_record(ok, "hmc", "hmc_beats_rwm_iat_highdim", detail);
    fp = fopen("results/hmc_vs_mh.csv", "w");
    if (fp) {
        fprintf(fp, "algo,d,n_keep,iat0,accept_rate,mean0,se_mean\n");
        fprintf(fp, "rwm,%d,%d,%.3f,%.4f,%.6f,%.6f\n",
                d, n_keep, iat_r, ch_r.accept_rate, mh[0], se_r);
        fprintf(fp, "hmc,%d,%d,%.3f,%.4f,%.6f,%.6f\n",
                d, n_keep, iat_h, ch_h.accept_rate, hh[0], se_h);
        fclose(fp);
    }
    mlab_chain_free(&ch_r);
    mlab_chain_free(&ch_h);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"mh", "diag_gaussian_moments",
         "RWM 采样 2D 对角高斯：均值<3SE，协方差 rel<10%",
         t_mh_diag_gaussian_moments},
        {"mh", "correlated_gaussian_rw",
         "RWM 采样 ρ=0.95 高斯：矩对账与接受率",
         t_mh_correlated_gaussian_rw},
        {"mh", "thin_reduces_stored_acf",
         "thin 降低存盘样本自相关（thin 必要性）",
         t_mh_thin_reduces_stored_acf},
        {"gibbs", "conjugate_bvn_analytical",
         "Gibbs 共轭二元正态后验 vs 解析矩",
         t_gibbs_conjugate_analytical},
        {"gibbs", "vs_mh_iat_ratio",
         "同目标 Gibbs IAT / RWM IAT < 1/3",
         t_gibbs_vs_mh_iat_ratio},
        {"hmc", "hmc_beats_rwm_iat_highdim",
         "d=16 HMC 的 IAT 显著短于 RWM（注记项落地）",
         t_hmc_beats_rwm_iat_highdim},
        {"diagnostics", "gelman_rubin_4chain",
         "4 链 Gelman-Rubin：burn-in 后 R̂<1.1 持续",
         t_gelman_rubin_4chain},
        {"diagnostics", "iat_independent_mh_reference",
         "独立 MH（提议≈目标）IAT 近 1 的参照",
         t_iat_white_noise_reference},
    };
    test_ensure_results_dir();
    return test_run_main("C2-mcmc", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
