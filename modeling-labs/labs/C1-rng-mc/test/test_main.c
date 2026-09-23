/*
 * C1: rng 质量 / 逆变换 / 拒绝采样 / MC / IS 诊断 / Sobol
 * 判据（路线图 C1）:
 *  - 卡方均匀性拒绝率在理论区间；滞后自相关 |ρ| < 2/√N
 *  - 逆变换 KS 拒绝率 ∈ [0.03,0.07]（1000 次，α=0.05）
 *  - 拒绝采样接受率 vs 理论 rel < 2%
 *  - MC 斜率 −0.5±0.05；IS 方差更小；权重 ESS/坏比值诊断
 *  - Sobol（注记项）：与参考实现逐点一致；同 N 积分误差显著小于伪随机
 */
#include "harness.h"
#include "lab.h"
#include "rng.h"
#include "mc.h"
#include "dist.h"
#include "stats.h"
#include "gof.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

/* ---------- suite: rng ---------- */

static int t_rng_kinds_uniform_in_unit(void)
{
    const int kinds[] = {MLAB_RNG_SPLITMIX64, MLAB_RNG_XORSHIFT64STAR,
                         MLAB_RNG_LCG, MLAB_RNG_MT19937};
    int ki, all_ok = 1;
    char detail[128] = {0};
    for (ki = 0; ki < 4; ++ki) {
        mlab_rng r;
        int i, bad = 0;
        mlab_rng_seed_kind(&r, 42, kinds[ki]);
        for (i = 0; i < 5000; ++i) {
            double u = mlab_rng_uniform(&r);
            if (!(u > 0 && u < 1)) ++bad;
        }
        if (bad) all_ok = 0;
        snprintf(detail + strlen(detail), sizeof detail - strlen(detail),
                 "%s:%d ", mlab_rng_kind_name(kinds[ki]), bad);
    }
    test_record(all_ok, "rng", "all_kinds_open_unit_interval", detail);
    return all_ok;
}

/* E1：四种发生器卡方均匀性拒绝率 + 滞后自相关（|ρ|<2/√N）。
 * 结果落盘 results/rng_quality.csv。 */
static int t_rng_kinds_quality(void)
{
    const int kinds[] = {MLAB_RNG_SPLITMIX64, MLAB_RNG_XORSHIFT64STAR,
                         MLAB_RNG_LCG, MLAB_RNG_MT19937};
    const int chi_reps = 600, chi_n = 4000, chi_k = 16;
    const int acf_n = 50000, acf_maxlag = 40;
    double bound = 2.0 / sqrt((double)acf_n);
    int ki, all_ok = 1;
    FILE *fp;
    char detail[192] = {0};

    fp = fopen("results/rng_quality.csv", "w");
    if (fp)
        fprintf(fp, "kind,chi2_reject_rate,chi2_reps,n_chi,k_bins,"
                    "acf_n,acf_lag1,acf_max,acf_bound2sig,acf_exceed_frac\n");
    for (ki = 0; ki < 4; ++ki) {
        mlab_rng rng;
        int rep, rej = 0, exceed = 0, ok;
        double acf1, acfmax;
        int argmax = 0;
        mlab_rng_seed_kind(&rng, 1500 + (unsigned)ki, kinds[ki]);
        for (rep = 0; rep < chi_reps; ++rep) {
            double p = 0;
            mlab_rng_chi2_uniform(&rng, chi_n, chi_k, &p);
            if (p < 0.05) ++rej;
        }
        mlab_rng_seed_kind(&rng, 1600 + (unsigned)ki, kinds[ki]);
        {
            /* acf1 与超界比例（2/√N 口径） */
            double *x = (double *)malloc((size_t)acf_n * sizeof(double));
            int lag;
            if (!x) return 0;
            for (lag = 0; lag < acf_n; ++lag) x[lag] = mlab_rng_uniform(&rng);
            acf1 = mlab_autocorr(x, acf_n, 1);
            acfmax = 0.0;
            for (lag = 1; lag <= acf_maxlag; ++lag) {
                double a = fabs(mlab_autocorr(x, acf_n, lag));
                if (a > bound) ++exceed;
                if (a > acfmax) {
                    acfmax = a;
                    argmax = lag;
                }
            }
            free(x);
        }
        {
            double rate = (double)rej / chi_reps;
            double frac = (double)exceed / acf_maxlag;
            ok = rate >= 0.03 && rate <= 0.07
                 && fabs(acf1) < bound && frac <= 0.15;
            if (!ok) all_ok = 0;
            snprintf(detail + strlen(detail), sizeof detail - strlen(detail),
                     "%s chi2=%.3f acf1=%.4f max=%.4f(lag%d) frac=%.2f | ",
                     mlab_rng_kind_name(kinds[ki]), rate, acf1, acfmax,
                     argmax, frac);
            if (fp)
                fprintf(fp, "%s,%.4f,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.4f\n",
                        mlab_rng_kind_name(kinds[ki]), rate, chi_reps,
                        chi_n, chi_k, acf_n, acf1, acfmax, bound, frac);
        }
    }
    if (fp) fclose(fp);
    test_record(all_ok, "rng", "kinds_chi2_and_acf_quality", detail);
    return all_ok;
}

/* 正态采样质量（矩 + 单次 KS）：method=0 Box-Muller，1 Marsaglia 极法 */
static double skew_kurt(const double *x, int n, double mean, double sd,
                        double *kurt_out)
{
    double s3 = 0.0, s4 = 0.0;
    int i;
    for (i = 0; i < n; ++i) {
        double z = (x[i] - mean) / sd;
        s3 += z * z * z;
        s4 += z * z * z * z;
    }
    *kurt_out = s4 / n - 3.0;
    return s3 / n;
}

static double cdf_std_normal(double x, void *ctx)
{
    (void)ctx;
    return mlab_normal_cdf(x, 0.0, 1.0);
}

static int normal_quality(int polar, unsigned seed)
{
    const int n = 40000;
    double *x = (double *)malloc((size_t)n * sizeof(double));
    mlab_rng rng;
    double mean, sd, sk, ku, ks_d, crit;
    int i, ok;
    FILE *fp;
    char d[160];
    if (!x) return 0;
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    for (i = 0; i < n; ++i)
        x[i] = polar ? mlab_rng_normal_polar(&rng) : mlab_rng_normal(&rng);
    mean = mlab_mean(x, n);
    sd = mlab_std(x, n);
    sk = skew_kurt(x, n, mean, sd, &ku);
    mlab_sort_asc(x, n);
    ks_d = mlab_ks_statistic(x, n, cdf_std_normal, NULL);
    crit = mlab_ks_crit_alpha05(n);
    ok = fabs(mean) < 0.02 && fabs(sd - 1.0) < 0.02
         && fabs(sk) < 0.05 && fabs(ku) < 0.10 && ks_d < crit;
    snprintf(d, sizeof d,
             "mean=%.4f sd=%.4f skew=%.4f kurt=%.4f KS=%.4f (crit %.4f)",
             mean, sd, sk, ku, ks_d, crit);
    fp = fopen("results/normal_quality.csv", polar ? "a" : "w");
    if (fp) {
        if (!polar)
            fprintf(fp, "method,mean,sd,skew,kurt,ks_d,ks_crit,n\n");
        fprintf(fp, "%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d\n",
                polar ? "marsaglia_polar" : "box_muller",
                mean, sd, sk, ku, ks_d, crit, n);
        fclose(fp);
    }
    free(x);
    return ok;
}

static int t_normal_box_muller_quality(void)
{
    int ok = normal_quality(0, 2101);
    test_record(ok, "rng", "normal_box_muller_quality",
                ok ? "矩与 KS 过关" : "矩或 KS 失败");
    return ok;
}

static int t_normal_marsaglia_polar_quality(void)
{
    int ok = normal_quality(1, 2102);
    test_record(ok, "rng", "normal_marsaglia_polar_quality",
                ok ? "矩与 KS 过关" : "矩或 KS 失败");
    return ok;
}

/* 离散分布逆变换：5 类权重，chi2 GOF 拒绝率应落在理论区间 */
static int t_discrete_inverse_transform(void)
{
    const double p[5] = {0.08, 0.15, 0.27, 0.30, 0.20};
    const int reps = 300, n = 600;
    mlab_rng rng;
    int rep, rej = 0;
    double expn[5];
    for (rep = 0; rep < 5; ++rep) expn[rep] = (double)n * p[rep];
    mlab_rng_seed_kind(&rng, 2201, MLAB_RNG_SPLITMIX64);
    for (rep = 0; rep < reps; ++rep) {
        long obs[5] = {0, 0, 0, 0, 0};
        int i, cat;
        double chi2;
        for (i = 0; i < n; ++i) {
            cat = mlab_rng_discrete(&rng, p, 5);
            if (cat < 0 || cat > 4) break;
            obs[cat] += 1;
        }
        chi2 = mlab_chi2_gof(obs, expn, 5);
        if (mlab_chi2_sf(chi2, 4) < 0.05) ++rej;
    }
    {
        double rate = (double)rej / reps;
        int ok = rate >= 0.03 && rate <= 0.07;
        char d[64];
        snprintf(d, sizeof d, "discrete inv-CDF chi2 reject=%.3f", rate);
        test_record(ok, "rng", "discrete_inverse_chi2_type1", d);
        return ok;
    }
}

/* 间隔检验：命中间隔 ~ 截尾几何，chi2 GOF 拒绝率在理论区间 */
static int t_gap_test_geometric(void)
{
    const int reps = 300, n = 40000, k = 5;
    const double alpha = 0.3;
    mlab_rng rng;
    int rep, rej = 0;
    mlab_rng_seed_kind(&rng, 2301, MLAB_RNG_SPLITMIX64);
    for (rep = 0; rep < reps; ++rep) {
        double pval = 0;
        mlab_rng_gap_chi2(&rng, n, alpha, k, &pval);
        if (pval > 0.0 && pval < 0.05) ++rej;
        else if (pval <= 0.0) ++rej; /* 计算失败视为异常 */
    }
    {
        double rate = (double)rej / reps;
        int ok = rate >= 0.03 && rate <= 0.07;
        char d[72];
        snprintf(d, sizeof d, "gap test (a=0.3,k=5) reject=%.3f", rate);
        test_record(ok, "rng", "gap_test_chi2_type1", d);
        return ok;
    }
}

/* ---------- suite: inverse_transform ---------- */

/* KS 一类错误率：method=0 指数，1 正态（均走逆变换正本） */
static int ks_type1_rate(int normal, unsigned seed)
{
    mlab_rng rng;
    double *x = malloc(sizeof(double) * 200);
    int r, i, rej = 0;
    const int reps = 1000, n = 200;
    double crit = mlab_ks_crit_alpha05(n);
    if (!x) return 0;
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    for (r = 0; r < reps; ++r) {
        double dmax = 0;
        for (i = 0; i < n; ++i)
            x[i] = normal ? mlab_rng_normal_inv(&rng)
                          : mlab_expon_quantile(mlab_rng_uniform(&rng), 1.0);
        mlab_sort_asc(x, n);
        for (i = 0; i < n; ++i) {
            double Fx = normal ? mlab_normal_cdf(x[i], 0, 1)
                               : mlab_expon_cdf(x[i], 1.0);
            double hi = (double)(i + 1) / n, lo = (double)i / n;
            if (fabs(hi - Fx) > dmax) dmax = fabs(hi - Fx);
            if (fabs(Fx - lo) > dmax) dmax = fabs(Fx - lo);
        }
        if (dmax > crit) ++rej;
    }
    free(x);
    return rej;
}

static int t_inv_expon_ks_rate(void)
{
    int rej = ks_type1_rate(0, 301);
    double rate = (double)rej / 1000;
    int ok = rate >= 0.03 && rate <= 0.07;
    char d[64];
    snprintf(d, sizeof d, "Exp inv-CDF KS reject=%.3f (1000 reps)", rate);
    test_record(ok, "inverse_transform", "exponential_ks_type1", d);
    return ok;
}

static int t_inv_normal_ks_rate(void)
{
    int rej = ks_type1_rate(1, 302);
    double rate = (double)rej / 1000;
    int ok = rate >= 0.03 && rate <= 0.07;
    char d[64];
    snprintf(d, sizeof d, "Normal inv-CDF KS reject=%.3f (1000 reps)", rate);
    test_record(ok, "inverse_transform", "normal_ks_type1", d);
    return ok;
}

static int t_inv_normal_roundtrip_moments(void)
{
    mlab_rng r;
    int n = 8000, i;
    double *x = malloc(sizeof(double) * (size_t)n);
    int ok = 0;
    if (!x) return 0;
    mlab_rng_seed_kind(&r, 33, MLAB_RNG_SPLITMIX64);
    for (i = 0; i < n; ++i) x[i] = mlab_rng_normal_inv(&r);
    ok = fabs(mlab_mean(x, n)) < 0.04 && fabs(mlab_std(x, n) - 1) < 0.04;
    {
        char d[64];
        snprintf(d, sizeof d, "mean=%.4f sd=%.4f", mlab_mean(x, n), mlab_std(x, n));
        test_record(ok, "inverse_transform", "normal_inv_moments", d);
    }
    free(x);
    return ok;
}

/* ---------- suite: rejection ---------- */

static double phi(double x, void *ctx) { (void)ctx; return mlab_normal_pdf(x, 0, 1); }
static double unif_s(mlab_rng *r, void *ctx)
{
    const double *lh = ctx;
    return lh[0] + (lh[1] - lh[0]) * mlab_rng_uniform(r);
}
static double unif_p(double x, void *ctx)
{
    const double *lh = ctx;
    return (x < lh[0] || x > lh[1]) ? 0.0 : 1.0 / (lh[1] - lh[0]);
}

static int t_rejection_accept_rate(void)
{
    const double lh[2] = {-2, 2};
    double c = mlab_normal_pdf(0, 0, 1) * 4.0;
    double theory = (mlab_normal_cdf(2, 0, 1) - mlab_normal_cdf(-2, 0, 1)) / c;
    mlab_rng rng;
    long trials = 0, acc = 0, want = 20000;
    double emp, rel;
    mlab_rng_seed_kind(&rng, 401, MLAB_RNG_SPLITMIX64);
    while (acc < want && trials < 5000000) {
        double s = 0;
        if (!mlab_rejection_sample(&rng, phi, NULL, unif_s, (void *)lh,
                                   unif_p, (void *)lh, c, 100000, &s, &trials))
            break;
        ++acc;
    }
    emp = trials ? (double)acc / (double)trials : 0;
    rel = theory > 0 ? fabs(emp - theory) / theory : 1;
    {
        int ok = rel < 0.02;
        char d[80];
        snprintf(d, sizeof d, "emp=%.5f theory=%.5f rel=%.2f%%", emp, theory, 100 * rel);
        test_record(ok, "rejection", "truncated_normal_accept_rate", d);
        return ok;
    }
}

/* ---------- suite: mc ---------- */

static int in_circle(const double *x, int d, void *ctx)
{
    (void)d; (void)ctx;
    return x[0] * x[0] + x[1] * x[1] <= 1.0;
}

static int t_mc_pi_reasonable(void)
{
    mlab_rng rng;
    mlab_mc_result out;
    double lb[2] = {0, 0}, ub[2] = {1, 1};
    double p, est;
    mlab_rng_seed_kind(&rng, 5001, MLAB_RNG_SPLITMIX64);
    p = mlab_mc_hit_miss(&rng, in_circle, NULL, 2, lb, ub, 20000, &out);
    est = 4.0 * p;
    {
        int ok = fabs(est - PI) < 0.08;
        char d[64];
        snprintf(d, sizeof d, "pi≈%.4f (true %.4f) se=%.4f", est, PI, 4 * out.se);
        test_record(ok, "mc", "hitmiss_pi_n20000", d);
        return ok;
    }
}

static double f_g(const double *x, int d, void *ctx)
{
    (void)d; (void)ctx;
    return exp(-x[0] * x[0]);
}

static double loglog_slope(const double *N, const double *rms, int m)
{
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int i;
    for (i = 0; i < m; ++i) {
        double x = log(N[i]), y = log(rms[i]);
        sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    return (m * sxy - sx * sy) / (m * sxx - sx * sx);
}

/* 收敛斜率（判据 −0.5±0.05）：π 命中法与 ∫e^{-x²}，结果落盘 mc_slope.csv */
static int t_mc_slope_pi_gauss(void)
{
    enum { M = 8, R = 40 };
    double Ns[M], rms_pi[M], rms_g[M];
    double exact_pi = PI, exact_g = sqrt(PI) * mlab_erf(1.0);
    double lb2[2] = {0, 0}, ub2[2] = {1, 1};
    double lb1[1] = {-1}, ub1[1] = {1};
    int i, rep, ok;
    double slope_pi, slope_g;
    FILE *fp;
    for (i = 0; i < M; ++i) Ns[i] = 256.0 * pow(2.0, (double)i);
    for (i = 0; i < M; ++i) {
        double sse_pi = 0, sse_g = 0;
        for (rep = 0; rep < R; ++rep) {
            mlab_rng rng;
            mlab_mc_result o;
            double e;
            /* 种子去重叠：格点距 1024 > R，保证 N 之间无种子复用 */
            mlab_rng_seed_kind(&rng, 900000u + (unsigned)i * 1024u + (unsigned)rep,
                               MLAB_RNG_SPLITMIX64);
            e = mlab_mc_hit_miss(&rng, in_circle, NULL, 2, lb2, ub2,
                                 (long)Ns[i], &o);
            sse_pi += (4.0 * e - exact_pi) * (4.0 * e - exact_pi);
            mlab_rng_seed_kind(&rng, 910000u + (unsigned)i * 1024u + (unsigned)rep,
                               MLAB_RNG_SPLITMIX64);
            e = mlab_mc_box(&rng, f_g, NULL, 1, lb1, ub1, (long)Ns[i], &o);
            sse_g += (e - exact_g) * (e - exact_g);
        }
        rms_pi[i] = sqrt(sse_pi / R);
        rms_g[i] = sqrt(sse_g / R);
    }
    slope_pi = loglog_slope(Ns, rms_pi, M);
    slope_g = loglog_slope(Ns, rms_g, M);
    ok = fabs(slope_pi + 0.5) <= 0.05 && fabs(slope_g + 0.5) <= 0.05;
    fp = fopen("results/mc_slope.csv", "w");
    if (fp) {
        fprintf(fp, "problem,N,rms_error,exact,se_mean\n");
        for (i = 0; i < M; ++i) {
            fprintf(fp, "pi_hitmiss,%d,%.8f,%.8f,\n", (int)Ns[i], rms_pi[i], exact_pi);
            fprintf(fp, "gauss_box,%d,%.8f,%.8f,\n", (int)Ns[i], rms_g[i], exact_g);
        }
        fclose(fp);
    }
    {
        char d[96];
        snprintf(d, sizeof d, "slope_pi=%.4f slope_gauss=%.4f (want -0.5±0.05)",
                 slope_pi, slope_g);
        test_record(ok, "mc", "pi_gauss_convergence_slope", d);
    }
    return ok;
}

static int t_mc_box_known_integral(void)
{
    /* ∫_0^1 x^2 dx = 1/3 */
    mlab_rng rng;
    double est;
    int i;
    double s = 0;
    mlab_rng_seed_kind(&rng, 12, MLAB_RNG_SPLITMIX64);
    for (i = 0; i < 20000; ++i) {
        double u = mlab_rng_uniform(&rng);
        s += u * u;
    }
    est = s / 20000.0;
    {
        int ok = fabs(est - 1.0 / 3.0) < 0.01;
        char d[48];
        snprintf(d, sizeof d, "∫x^2≈%.4f want 0.3333", est);
        test_record(ok, "mc", "box_quadratic_unit_interval", d);
        return ok;
    }
}

/* ---------- suite: importance_sampling ---------- */

static double f_e(double x, void *c) { (void)c; return exp(x); }
static double q_e(double x, void *c)
{
    (void)c;
    if (x < 0 || x > 1) return 0;
    return exp(0.5 * x) / (2.0 * (sqrt(exp(1.0)) - 1.0));
}
static double s_e(mlab_rng *r, void *c)
{
    (void)c;
    return 2.0 * log(1.0 + mlab_rng_uniform(r) * (sqrt(exp(1.0)) - 1.0));
}

static int t_is_lower_variance(void)
{
    const long N = 3000;
    const int R = 80;
    double *plain = malloc(sizeof(double) * R);
    double *isv = malloc(sizeof(double) * R);
    int r, ok = 0;
    double vp, vi;
    FILE *fp;
    if (!plain || !isv) { free(plain); free(isv); return 0; }
    for (r = 0; r < R; ++r) {
        mlab_rng rng;
        mlab_mc_result o;
        mlab_rng_seed_kind(&rng, 7000 + r, MLAB_RNG_SPLITMIX64);
        {
            double s = 0;
            int i;
            for (i = 0; i < N; ++i) s += exp(mlab_rng_uniform(&rng));
            plain[r] = s / N;
        }
        mlab_rng_seed_kind(&rng, 8000 + r, MLAB_RNG_SPLITMIX64);
        mlab_mc_is(&rng, f_e, NULL, q_e, NULL, s_e, NULL, N, &o);
        isv[r] = o.mean;
    }
    vp = mlab_var(plain, R);
    vi = mlab_var(isv, R);
    ok = vi < 0.6 * vp;
    fp = fopen("results/is_variance.csv", "w");
    if (fp) {
        fprintf(fp, "exact,n,R,mean_plain,mean_is,var_plain,var_is,var_ratio\n");
        fprintf(fp, "1.71828183,%ld,%d,,,,%.6e,\n", N, R, vp > 0 ? vi / vp : 0);
        fclose(fp);
    }
    {
        char d[64];
        snprintf(d, sizeof d, "var_is/var_plain=%.3f", vp > 0 ? vi / vp : 0);
        test_record(ok, "importance_sampling", "exp_integral_var_ratio", d);
    }
    free(plain); free(isv);
    return ok;
}

/* E_p[f]，p=U(0,1)：提议 q∝e^{x/2}（好提议），权重诊断应全绿 */
static int t_is_diag_good_proposal(void)
{
    mlab_rng rng;
    mlab_mc_result out;
    mlab_mc_is_diag diag;
    double est;
    int ok;
    FILE *fp;
    mlab_rng_seed_kind(&rng, 7500, MLAB_RNG_SPLITMIX64);
    {
        static const double unit[2] = {0.0, 1.0};
        est = mlab_mc_is_weighted(&rng, unif_p, (void *)unit, f_e, NULL,
                                  q_e, NULL, s_e, NULL, 20000, &out, &diag);
    }
    ok = fabs(est - (exp(1.0) - 1.0)) < 4.0 * out.se + 1e-3
         && diag.ess_ratio > 0.90
         && diag.w_cv2 < 0.15
         && diag.w_max_ratio < 0.01;
    fp = fopen("results/is_weight_diag.csv", "w");
    if (fp) {
        fprintf(fp, "proposal,n,ess_ratio,w_cv2,w_max_ratio,mean,se\n");
        fprintf(fp, "good_q_exp_half,%.0f,%.6f,%.6f,%.6f,%.6f,%.2e\n",
                (double)out.n, diag.ess_ratio, diag.w_cv2,
                diag.w_max_ratio, est, out.se);
        fclose(fp);
    }
    {
        char d[96];
        snprintf(d, sizeof d,
                 "est=%.4f±%.4f ess/n=%.3f cv2=%.4f maxw=%.4f",
                 est, out.se, diag.ess_ratio, diag.w_cv2, diag.w_max_ratio);
        test_record(ok, "importance_sampling", "weight_diag_good_proposal", d);
    }
    return ok;
}

/* 坏提议：q=N(0,0.5²) 窄于 p=N(0,1)，尾部权重爆炸 → ESS 崩塌、坏比值现形 */
static double p_normal(double x, void *c) { (void)c; return mlab_normal_pdf(x, 0, 1); }
static double q_narrow(double x, void *c)
{
    (void)c;
    return mlab_normal_pdf(x, 0, 0.5);
}
static double s_narrow(mlab_rng *r, void *c)
{
    (void)c;
    return 0.5 * mlab_rng_normal(r);
}
static double f_one(double x, void *c) { (void)c; (void)x; return 1.0; }

static int t_is_diag_bad_proposal(void)
{
    mlab_rng rng;
    mlab_mc_result out;
    mlab_mc_is_diag diag;
    double est;
    int ok;
    FILE *fp;
    mlab_rng_seed_kind(&rng, 7600, MLAB_RNG_SPLITMIX64);
    est = mlab_mc_is_weighted(&rng, p_normal, NULL, f_one, NULL,
                              q_narrow, NULL, s_narrow, NULL, 20000, &out, &diag);
    /* 坏比值诊断应报警：ESS 比率显著低于好提议，单权重占比偏高 */
    ok = fabs(est - 1.0) < 0.05
         && diag.ess_ratio < 0.60
         && diag.w_max_ratio > 0.01;
    fp = fopen("results/is_weight_diag.csv", "a");
    if (fp) {
        fprintf(fp, "bad_q_narrow,%.0f,%.6f,%.6f,%.6f,%.6f,%.2e\n",
                (double)out.n, diag.ess_ratio, diag.w_cv2,
                diag.w_max_ratio, est, out.se);
        fclose(fp);
    }
    {
        char d[96];
        snprintf(d, sizeof d,
                 "est=%.4f ess/n=%.3f cv2=%.2f maxw=%.4f (诊断应报警)",
                 est, diag.ess_ratio, diag.w_cv2, diag.w_max_ratio);
        test_record(ok, "importance_sampling", "weight_diag_bad_proposal", d);
    }
    return ok;
}

/* ---------- suite: qmc（Sobol 注记项） ---------- */

/* 与 scipy.stats.qmc.Sobol(d=2, scramble=False) 前 16 点逐点对账 */
static int t_sobol_first_points_reference(void)
{
    static const double ref[16][2] = {
        {0.0, 0.0}, {0.5, 0.5}, {0.75, 0.25}, {0.25, 0.75},
        {0.375, 0.375}, {0.875, 0.875}, {0.625, 0.125}, {0.125, 0.625},
        {0.1875, 0.3125}, {0.6875, 0.8125}, {0.9375, 0.0625},
        {0.4375, 0.5625}, {0.3125, 0.1875}, {0.8125, 0.6875},
        {0.5625, 0.4375}, {0.0625, 0.9375}
    };
    double x[2];
    int i, ok = 1;
    double maxdiff = 0.0;
    for (i = 0; i < 16; ++i) {
        if (!mlab_sobol_point(2, i, x)) return 0;
        {
            double dd = fabs(x[0] - ref[i][0]);
            if (dd > maxdiff) maxdiff = dd;
            dd = fabs(x[1] - ref[i][1]);
            if (dd > maxdiff) maxdiff = dd;
        }
    }
    ok = maxdiff < 1e-12;
    {
        char d[64];
        snprintf(d, sizeof d, "max|diff|=%.2e vs scipy 16 pts", maxdiff);
        test_record(ok, "qmc", "sobol_first_points_match_reference", d);
    }
    return ok;
}

static double f_g2(const double *x, int d, void *ctx)
{
    (void)d; (void)ctx;
    return exp(-x[0] * x[0] - x[1] * x[1]);
}

/* 同 N 对比：Sobol 积分误差显著小于伪随机 MC（低差异 vs O(1/√N)） */
static int t_sobol_beats_mc_2d(void)
{
    const double exact = PI * mlab_erf(1.0) * mlab_erf(1.0); /* ∫_{-1}^1∫ e^{-x²-y²} */
    const int Ns[] = {256, 1024, 4096, 16384};
    const int nN = 4, R = 20;
    int i, r, ok = 1;
    double sob_err[4], mc_rms[4];
    FILE *fp;
    for (i = 0; i < nN; ++i) {
        long n = Ns[i];
        double s = 0.0, sse = 0.0;
        for (r = 0; r < (int)n; ++r) {
            double x[2];
            mlab_sobol_point(2, r + 1, x); /* 跳过原点 */
            s += exp(-(2.0 * x[0] - 1.0) * (2.0 * x[0] - 1.0)
                     - (2.0 * x[1] - 1.0) * (2.0 * x[1] - 1.0));
        }
        sob_err[i] = fabs(4.0 * s / (double)n - exact);
        for (r = 0; r < R; ++r) {
            mlab_rng rng;
            mlab_mc_result o;
            double lb[2] = {-1, -1}, ub[2] = {1, 1};
            double e;
            mlab_rng_seed_kind(&rng, 990000u + (unsigned)i * 512u + (unsigned)r,
                               MLAB_RNG_SPLITMIX64);
            e = mlab_mc_box(&rng, f_g2, NULL, 2, lb, ub, n, &o);
            sse += (e - exact) * (e - exact);
        }
        mc_rms[i] = sqrt(sse / R);
        if (!(sob_err[i] < 0.5 * mc_rms[i])) ok = 0;
    }
    fp = fopen("results/sobol_convergence.csv", "w");
    if (fp) {
        fprintf(fp, "N,sobol_abs_err,mc_rms_err,exact\n");
        for (i = 0; i < nN; ++i)
            fprintf(fp, "%d,%.3e,%.3e,%.8f\n", Ns[i], sob_err[i], mc_rms[i], exact);
        fclose(fp);
    }
    {
        char d[112];
        snprintf(d, sizeof d,
                 "N=4096: sobol=%.2e mc=%.2e (need sobol<0.5·mc 全档)",
                 sob_err[2], mc_rms[2]);
        test_record(ok, "qmc", "sobol_beats_mc_2d", d);
    }
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"rng", "all_kinds_open_unit_interval", "四种发生器均匀落在(0,1)", t_rng_kinds_uniform_in_unit},
        {"rng", "kinds_chi2_and_acf_quality", "四发生器卡方拒绝率+自相关(2/√N)", t_rng_kinds_quality},
        {"rng", "normal_box_muller_quality", "Box-Muller 矩与 KS", t_normal_box_muller_quality},
        {"rng", "normal_marsaglia_polar_quality", "Marsaglia 极法矩与 KS", t_normal_marsaglia_polar_quality},
        {"rng", "discrete_inverse_chi2_type1", "离散逆变换卡方一类错误率", t_discrete_inverse_transform},
        {"rng", "gap_test_chi2_type1", "间隔检验(截尾几何)一类错误率", t_gap_test_geometric},
        {"inverse_transform", "exponential_ks_type1", "逆变换指数 KS 类型 I(1000次)", t_inv_expon_ks_rate},
        {"inverse_transform", "normal_ks_type1", "逆变换正态 KS 类型 I(1000次)", t_inv_normal_ks_rate},
        {"inverse_transform", "normal_inv_moments", "逆变换正态矩", t_inv_normal_roundtrip_moments},
        {"rejection", "truncated_normal_accept_rate", "拒绝采样接受率 vs 理论", t_rejection_accept_rate},
        {"mc", "hitmiss_pi_n20000", "命中法估 π", t_mc_pi_reasonable},
        {"mc", "box_quadratic_unit_interval", "盒积分 x^2", t_mc_box_known_integral},
        {"mc", "pi_gauss_convergence_slope", "π/高斯积分收敛斜率 −0.5±0.05", t_mc_slope_pi_gauss},
        {"importance_sampling", "exp_integral_var_ratio", "IS 方差比朴素 MC", t_is_lower_variance},
        {"importance_sampling", "weight_diag_good_proposal", "IS 权重诊断(好提议 ESS 高)", t_is_diag_good_proposal},
        {"importance_sampling", "weight_diag_bad_proposal", "IS 权重诊断(坏提议 ESS 崩塌)", t_is_diag_bad_proposal},
        {"qmc", "sobol_first_points_match_reference", "Sobol 前 16 点与参考实现一致", t_sobol_first_points_reference},
        {"qmc", "sobol_beats_mc_2d", "同 N 下 Sobol 积分误差小于伪随机", t_sobol_beats_mc_2d},
    };
    test_ensure_results_dir();
    return test_run_main("C1-rng-mc", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
