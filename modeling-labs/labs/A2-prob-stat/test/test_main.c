/*
 * A2 tests: dist / stats / gof / rng / clt / control_variate / estimation / variance_reduction
 * CLI: bin/test [suite|suite/case ...]
 * 全量运行（无参数）时重算 results/*.csv（固定 seed，确定性可复现）。
 */
#include "harness.h"
#include "dist.h"
#include "stats.h"
#include "gof.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- dist ---- */

static int t_dist_normal_cdf_sym(void)
{
    double p0 = mlab_normal_cdf(0.0, 0.0, 1.0);
    double p1 = mlab_normal_cdf(1.0, 0.0, 1.0);
    int ok = fabs(p0 - 0.5) < 1e-10 && fabs(p1 - (0.5 + 0.341344746)) < 1e-4;
    char d[96];
    snprintf(d, sizeof d, "Phi(0)=%.6f Phi(1)=%.6f", p0, p1);
    test_record(ok, "dist", "normal_cdf_landmarks", d);
    return ok;
}

static int t_dist_normal_quantile_roundtrip(void)
{
    double ps[] = {0.01, 0.1, 0.5, 0.9, 0.99};
    int i, bad = 0;
    for (i = 0; i < 5; ++i) {
        double x = mlab_normal_quantile(ps[i], 0.0, 1.0);
        double p = mlab_normal_cdf(x, 0.0, 1.0);
        if (fabs(p - ps[i]) > 1e-8) ++bad;
    }
    {
        char d[64];
        int ok = bad == 0;
        snprintf(d, sizeof d, "roundtrip mismatches=%d/5", bad);
        test_record(ok, "dist", "normal_quantile_roundtrip", d);
        return ok;
    }
}

static int t_dist_expon_quantile(void)
{
    double x = mlab_expon_quantile(0.5, 1.0);
    int ok = fabs(x - log(2.0)) < 1e-12;
    char d[64];
    snprintf(d, sizeof d, "Exp median=%.12f want ln2", x);
    test_record(ok, "dist", "expon_median_quantile", d);
    return ok;
}

static int t_dist_erf_landmarks(void)
{
    double e0 = mlab_erf(0.0);
    double e1 = mlab_erf(1.0);
    double c1 = mlab_erfc(1.0);
    int ok = e0 == 0.0 && fabs(e1 - 0.8427007929) < 1e-7 &&
             fabs(c1 - 0.1572992071) < 1e-7 &&
             fabs(mlab_erf(-1.0) + e1) < 1e-14;
    char d[96];
    snprintf(d, sizeof d, "erf(0)=%.1f erf(1)=%.9f erfc(1)=%.9f", e0, e1, c1);
    test_record(ok, "dist", "erf_landmarks", d);
    return ok;
}

/* ---- stats ---- */

static int t_stats_mean_var(void)
{
    double x[] = {1, 2, 3, 4};
    int ok = fabs(mlab_mean(x, 4) - 2.5) < 1e-15 && fabs(mlab_var(x, 4) - 5.0 / 3.0) < 1e-14;
    char d[80];
    snprintf(d, sizeof d, "mean=%.6f var=%.6f (want 2.5, 5/3)", mlab_mean(x, 4), mlab_var(x, 4));
    test_record(ok, "stats", "mean_var_small_sample", d);
    return ok;
}

static int t_stats_sort_ecdf(void)
{
    double x[] = {3, 1, 2};
    mlab_sort_asc(x, 3);
    {
        double e = mlab_ecdf(x, 3, 2.0);
        int ok = x[0] == 1 && x[1] == 2 && x[2] == 3 && fabs(e - 2.0 / 3.0) < 1e-15;
        char d[80];
        snprintf(d, sizeof d, "sorted=%.0f,%.0f,%.0f ecdf(2)=%.4f", x[0], x[1], x[2], e);
        test_record(ok, "stats", "sort_and_ecdf", d);
        return ok;
    }
}

/* ---- gof ---- */

/* KS 检验统一走 gof 正本 mlab_ks_statistic（C 下游实验室的裁判 API） */
static double cdf_unif(double x, void *ctx) { (void)ctx; return x; }
static double cdf_exp1(double x, void *ctx) { (void)ctx; return 1.0 - exp(-x); }
static double cdf_stdnorm(double x, void *ctx) { (void)ctx; return mlab_normal_cdf(x, 0.0, 1.0); }

typedef void (*mlab_fill_fn)(mlab_rng *r, double *buf, int n);

static void fill_unif(mlab_rng *r, double *buf, int n)
{
    int i;
    for (i = 0; i < n; ++i) buf[i] = mlab_rng_uniform(r);
}
static void fill_exp(mlab_rng *r, double *buf, int n)
{
    int i;
    for (i = 0; i < n; ++i) buf[i] = mlab_rng_exponential(r, 1.0);
}
static void fill_norm(mlab_rng *r, double *buf, int n)
{
    int i;
    for (i = 0; i < n; ++i) buf[i] = mlab_rng_normal(r);
}

/* 路线图 L138：1000 次重复，H0 真时拒绝率 ∈ [0.03, 0.07]（α=0.05） */
static double ks_reject_rate(mlab_cdf_fn F, void *fctx, mlab_fill_fn gen, int reps, int n)
{
    mlab_rng rng;
    double *buf = malloc(sizeof(double) * (size_t)n);
    int r, rej = 0;
    double crit = mlab_ks_crit_alpha05(n);
    if (!buf) return -1;
    mlab_rng_seed(&rng, 11);
    for (r = 0; r < reps; ++r) {
        double d;
        int i;
        gen(&rng, buf, n);
        mlab_sort_asc(buf, n);
        d = mlab_ks_statistic(buf, n, F, fctx);
        if (d > crit) ++rej;
    }
    free(buf);
    return (double)rej / reps;
}

static double g_ks_rate[3]; /* unif / exp / norm，供 CSV */

static int t_gof_ks_type1(void)
{
    int n = 200, reps = 1000;
    g_ks_rate[0] = ks_reject_rate(cdf_unif, NULL, fill_unif, reps, n);
    g_ks_rate[1] = ks_reject_rate(cdf_exp1, NULL, fill_exp, reps, n);
    g_ks_rate[2] = ks_reject_rate(cdf_stdnorm, NULL, fill_norm, reps, n);
    {
        int ok = g_ks_rate[0] >= 0.03 && g_ks_rate[0] <= 0.07;
        char d[96];
        snprintf(d, sizeof d, "unif=%.3f exp=%.3f norm=%.3f (1000 reps, need unif∈[0.03,0.07])",
                 g_ks_rate[0], g_ks_rate[1], g_ks_rate[2]);
        test_record(ok, "gof", "ks_type1_uniform", d);
        return ok;
    }
}

static int t_gof_chi2_uniform_bins(void)
{
    mlab_rng rng;
    long obs[10] = {0};
    double expn[10];
    int i, n = 10000;
    double chi2, p;
    mlab_rng_seed(&rng, 5);
    for (i = 0; i < n; ++i) {
        int b = (int)(mlab_rng_uniform(&rng) * 10);
        if (b < 0) b = 0;
        if (b > 9) b = 9;
        obs[b]++;
    }
    for (i = 0; i < 10; ++i) expn[i] = n / 10.0;
    chi2 = mlab_chi2_gof(obs, expn, 10);
    p = mlab_chi2_sf(chi2, 9);
    {
        int ok = p > 0.01; /* 不应在 α=0.01 拒绝真均匀 */
        char d[80];
        snprintf(d, sizeof d, "chi2=%.2f p=%.4f", chi2, p);
        test_record(ok, "gof", "chi2_uniform_large_n", d);
        return ok;
    }
}

/* ---- rng（含未覆盖 API 的矩/界校验） ---- */

static int t_rng_reproducible_seed(void)
{
    mlab_rng a, b;
    int i, same = 1;
    mlab_rng_seed(&a, 12345);
    mlab_rng_seed(&b, 12345);
    for (i = 0; i < 100; ++i)
        if (mlab_rng_uniform(&a) != mlab_rng_uniform(&b)) same = 0;
    test_record(same, "rng", "same_seed_reproducible", same ? "100 uniforms match" : "mismatch");
    return same;
}

static int t_rng_uniform_open_interval(void)
{
    mlab_rng r;
    int i, bad = 0;
    mlab_rng_seed(&r, 9);
    for (i = 0; i < 10000; ++i) {
        double u = mlab_rng_uniform(&r);
        if (!(u > 0.0 && u < 1.0)) ++bad;
    }
    {
        char d[48];
        int ok = bad == 0;
        snprintf(d, sizeof d, "out-of-(0,1)=%d/10000", bad);
        test_record(ok, "rng", "uniform_open_unit_interval", d);
        return ok;
    }
}

static int t_rng_normal_moments(void)
{
    mlab_rng r;
    int n = 20000, i;
    double *x = malloc(sizeof(double) * (size_t)n);
    double mu, sd;
    int ok = 0;
    if (!x) return 0;
    mlab_rng_seed(&r, 77);
    for (i = 0; i < n; ++i) x[i] = mlab_rng_normal(&r);
    mu = mlab_mean(x, n);
    sd = mlab_std(x, n);
    ok = fabs(mu) < 0.05 && fabs(sd - 1.0) < 0.05;
    {
        char d[80];
        snprintf(d, sizeof d, "n=%d mean=%.4f sd=%.4f", n, mu, sd);
        test_record(ok, "rng", "normal_first_two_moments", d);
    }
    free(x);
    return ok;
}

static int t_rng_normal_polar_moments(void)
{
    mlab_rng r;
    int n = 20000, i;
    double *x = malloc(sizeof(double) * (size_t)n);
    double mu, sd;
    int ok = 0;
    if (!x) return 0;
    mlab_rng_seed(&r, 78);
    for (i = 0; i < n; ++i) x[i] = mlab_rng_normal_polar(&r);
    mu = mlab_mean(x, n);
    sd = mlab_std(x, n);
    ok = fabs(mu) < 0.05 && fabs(sd - 1.0) < 0.05;
    {
        char d[80];
        snprintf(d, sizeof d, "polar n=%d mean=%.4f sd=%.4f", n, mu, sd);
        test_record(ok, "rng", "normal_polar_moments", d);
    }
    free(x);
    return ok;
}

static int t_rng_normal_inv_ks(void)
{
    mlab_rng r;
    const int n = 2000;
    double *x = malloc(sizeof(double) * (size_t)n);
    double d, crit;
    int i, ok = 0;
    if (!x) return 0;
    mlab_rng_seed(&r, 79);
    for (i = 0; i < n; ++i) x[i] = mlab_rng_normal_inv(&r);
    mlab_sort_asc(x, n);
    d = mlab_ks_statistic(x, n, cdf_stdnorm, NULL);
    crit = mlab_ks_crit_alpha05(n);
    ok = d < crit;
    {
        char d_[80];
        snprintf(d_, sizeof d_, "KS D=%.4f < crit=%.4f (逆变换 vs Φ)", d, crit);
        test_record(ok, "rng", "normal_inv_ks_vs_phi", d_);
    }
    free(x);
    return ok;
}

static int t_rng_poisson_moments(void)
{
    mlab_rng r;
    const int n = 20000;
    double *x = malloc(sizeof(double) * (size_t)n);
    double mu, sd;
    int i, ok = 0;
    if (!x) return 0;
    mlab_rng_seed(&r, 81);
    for (i = 0; i < n; ++i) x[i] = (double)mlab_rng_poisson(&r, 4.0);
    mu = mlab_mean(x, n);
    sd = mlab_std(x, n);
    ok = fabs(mu - 4.0) < 0.1 && fabs(sd - 2.0) < 0.1; /* Poisson: E=λ=4, SD=√λ=2 */
    {
        char d[80];
        snprintf(d, sizeof d, "Poisson(4) mean=%.4f sd=%.4f (want 4, 2)", mu, sd);
        test_record(ok, "rng", "poisson_moments", d);
    }
    free(x);
    return ok;
}

static int t_rng_bernoulli_rate(void)
{
    mlab_rng r;
    const int n = 20000;
    int i, ones = 0, ok;
    double p_hat;
    mlab_rng_seed(&r, 82);
    for (i = 0; i < n; ++i) ones += mlab_rng_bernoulli(&r, 0.3);
    p_hat = (double)ones / n;
    ok = fabs(p_hat - 0.3) < 0.02;
    {
        char d[64];
        snprintf(d, sizeof d, "Bernoulli(0.3) rate=%.4f", p_hat);
        test_record(ok, "rng", "bernoulli_rate", d);
    }
    return ok;
}

static int t_rng_chi2_uniform_api(void)
{
    mlab_rng r;
    double p = 0;
    double chi2 = mlab_rng_chi2_uniform(&r, 10000, 10, &p);
    int ok = p > 0.01;
    char d[80];
    snprintf(d, sizeof d, "api chi2=%.2f p=%.4f (真均匀不应被拒绝)", chi2, p);
    test_record(ok, "rng", "chi2_uniform_api", d);
    return ok;
}

static int t_rng_acf_within_bounds(void)
{
    mlab_rng r;
    double mx = mlab_rng_max_abs_acf(&r, 5000, 5, NULL);
    double bound = 3.0 / sqrt(5000.0);
    int ok = mx >= 0 && mx < bound;
    char d[80];
    snprintf(d, sizeof d, "max|acf(lag1..5)|=%.4f < 3/√n=%.4f", mx, bound);
    test_record(ok, "rng", "acf_within_bounds", d);
    return ok;
}

/* 计划批 1 A2④：补测 rng.c 此前未直测的 API */

static int t_rng_kind_name(void)
{
    int ok =
        strcmp(mlab_rng_kind_name(MLAB_RNG_SPLITMIX64), "splitmix64") == 0 &&
        strcmp(mlab_rng_kind_name(MLAB_RNG_XORSHIFT64STAR), "xorshift64*") == 0 &&
        strcmp(mlab_rng_kind_name(MLAB_RNG_LCG), "lcg-mmix") == 0 &&
        strcmp(mlab_rng_kind_name(MLAB_RNG_MT19937), "mt19937") == 0 &&
        strcmp(mlab_rng_kind_name(99), "unknown") == 0;
    char d[96];
    snprintf(d, sizeof d, "names=%s/%s/%s/%s unknown=%s",
             mlab_rng_kind_name(0), mlab_rng_kind_name(1),
             mlab_rng_kind_name(2), mlab_rng_kind_name(3),
             mlab_rng_kind_name(99));
    test_record(ok, "rng", "kind_name_labels", d);
    return ok;
}

static int t_rng_seed_kind_and_u64(void)
{
    /* seed_kind 写入 kind；同 kind 同 seed 可复现；不同 kind 序列不同；
       u64 可用且 uniform 落在 (0,1)。非法 kind 回退 splitmix64。 */
    mlab_rng a, b, c;
    unsigned long long ua[4], ub[4];
    int kinds[4] = {MLAB_RNG_SPLITMIX64, MLAB_RNG_XORSHIFT64STAR,
                    MLAB_RNG_LCG, MLAB_RNG_MT19937};
    int k, i, ok_kind = 1, ok_rep = 1, ok_diff = 0, ok_u = 1;
    char d[120];

    for (k = 0; k < 4; ++k) {
        mlab_rng_seed_kind(&a, 20260301ull + (unsigned long long)k, kinds[k]);
        mlab_rng_seed_kind(&b, 20260301ull + (unsigned long long)k, kinds[k]);
        if (a.kind != kinds[k]) ok_kind = 0;
        ua[k] = mlab_rng_u64(&a);
        ub[k] = mlab_rng_u64(&b);
        if (ua[k] != ub[k]) ok_rep = 0;
        for (i = 0; i < 200; ++i) {
            double u = mlab_rng_uniform(&a);
            if (!(u > 0.0 && u < 1.0)) ok_u = 0;
            if (mlab_rng_uniform(&b) != u) ok_rep = 0;
        }
    }
    for (k = 0; k < 4 && !ok_diff; ++k)
        for (i = k + 1; i < 4; ++i)
            if (ua[k] != ua[i]) { ok_diff = 1; break; }

    mlab_rng_seed_kind(&c, 42ull, 99);
    if (c.kind != MLAB_RNG_SPLITMIX64) ok_kind = 0;

    snprintf(d, sizeof d,
             "kind/repro/uniform/diff/kind=%d/%d/%d/%d/%d u64[0]=%016llx",
             ok_kind, ok_rep, ok_u, ok_diff, c.kind == MLAB_RNG_SPLITMIX64,
             (unsigned long long)ua[0]);
    {
        int ok = ok_kind && ok_rep && ok_u && ok_diff;
        test_record(ok, "rng", "seed_kind_and_u64_all_generators", d);
        return ok;
    }
}

static int t_rng_autocorr_direct(void)
{
    /* 解析序列 x=[1,2,3,4,5]，mean=3，偏差平方和=10
       rho(1)=4/10=0.4，rho(2)=-1/10=-0.1；lag<1 或样本不足返回 0 */
    const double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double r1 = mlab_autocorr(x, 5, 1);
    double r2 = mlab_autocorr(x, 5, 2);
    double r0 = mlab_autocorr(x, 5, 0);
    double rbig = mlab_autocorr(x, 5, 5);
    int ok = fabs(r1 - 0.4) < 1e-12 && fabs(r2 + 0.1) < 1e-12 &&
             r0 == 0.0 && rbig == 0.0;
    char d[96];
    snprintf(d, sizeof d, "rho1=%.4f (want 0.4) rho2=%.4f (want -0.1) lag0=%.1f lag5=%.1f",
             r1, r2, r0, rbig);
    test_record(ok, "rng", "autocorr_direct_analytic", d);
    return ok;
}

/* ---- CLT：矩 + 直方图卡方（路线图 L133 实验 1 完整落地） ---- */

static int t_clt_moments(void)
{
    const int n_sample = 30, n_means = 4000;
    mlab_rng rng;
    double *means = malloc(sizeof(double) * (size_t)n_means);
    int i, j, ok = 0;
    double se = 1.0 / sqrt((double)n_sample);
    if (!means) return 0;
    mlab_rng_seed(&rng, 20260214ULL);
    for (i = 0; i < n_means; ++i) {
        double s = 0;
        for (j = 0; j < n_sample; ++j) s += mlab_rng_exponential(&rng, 1.0);
        means[i] = s / n_sample;
    }
    ok = fabs(mlab_mean(means, n_means) - 1.0) < 4 * se / sqrt((double)n_means) + 1e-3 &&
         fabs(mlab_std(means, n_means) - se) < 0.15 * se;
    {
        char d[96];
        snprintf(d, sizeof d, "E[Xbar]=%.4f sd=%.4f vs theory 1 / %.4f",
                 mlab_mean(means, n_means), mlab_std(means, n_means), se);
        test_record(ok, "clt", "exp_means_match_clt", d);
    }
    free(means);
    return ok;
}

#define CLT_BINS 20
static long g_clt_obs[CLT_BINS];
static double g_clt_edges[CLT_BINS + 1];
static double g_clt_chi2, g_clt_p;
static double g_clt_muhat, g_clt_sigmahat;

static int t_clt_chi2_normal_fit(void)
{
    const int n_sample = 100, n_means = 4000;
    mlab_rng rng;
    double *means = malloc(sizeof(double) * (size_t)n_means);
    double expn[CLT_BINS];
    int i, j, ok = 0;
    if (!means) return 0;
    mlab_rng_seed(&rng, 20260215ULL);
    for (i = 0; i < n_means; ++i) {
        double s = 0;
        for (j = 0; j < n_sample; ++j) s += mlab_rng_exponential(&rng, 1.0);
        means[i] = (s / n_sample - 1.0) * sqrt((double)n_sample); /* 标准化 */
    }
    mlab_mle_normal(means, n_means, &g_clt_muhat, &g_clt_sigmahat);
    /* 等概率箱：edge_j = Φ^{-1}(j/m) */
    for (j = 0; j <= CLT_BINS; ++j)
        g_clt_edges[j] = (j == 0 || j == CLT_BINS)
                             ? (j == 0 ? -1e12 : 1e12)
                             : mlab_normal_quantile((double)j / CLT_BINS,
                                                    g_clt_muhat, g_clt_sigmahat);
    for (j = 0; j < CLT_BINS; ++j) { g_clt_obs[j] = 0; expn[j] = (double)n_means / CLT_BINS; }
    for (i = 0; i < n_means; ++i) {
        for (j = 0; j < CLT_BINS; ++j) {
            if (means[i] >= g_clt_edges[j] && means[i] < g_clt_edges[j + 1]) {
                ++g_clt_obs[j];
                break;
            }
        }
    }
    g_clt_chi2 = mlab_chi2_gof(g_clt_obs, expn, CLT_BINS);
    g_clt_p = mlab_chi2_sf(g_clt_chi2, CLT_BINS - 1 - 2); /* 估计 μ,σ：df 减 2 */
    ok = g_clt_p > 0.01;
    {
        char d[112];
        snprintf(d, sizeof d, "n=%d m=%.3f s=%.3f chi2=%.2f df=%d p=%.4f (n=100 时 CLT 拟合好)",
                 n_sample, g_clt_muhat, g_clt_sigmahat, g_clt_chi2, CLT_BINS - 3, g_clt_p);
        test_record(ok, "clt", "chi2_normal_fit_standardized", d);
    }
    free(means);
    return ok;
}

/* ---- 控制变量法（路线图 L139 判据：与理论相对误差 < 10%） ---- */

static double g_vr[4];    /* cv / antithetic / crn / stratified 实测 VR，供 CSV */
static double g_vr_th[4]; /* 对应理论值（crn/stratified 无闭式，置 NaN） */

static int t_cv_variance_reduction(void)
{
    const int N = 2000, R = 1500;
    mlab_rng rng;
    double *plain = malloc(sizeof(double) * (size_t)R);
    double *cv = malloc(sizeof(double) * (size_t)R);
    int r, i, ok = 0;
    double Ef = exp(1.0) - 1.0, Eg = 0.5, Efg = 1.0;
    double Varf = (exp(2.0) - 1.0) / 2.0 - Ef * Ef;
    double Varg = 1.0 / 12.0;
    double cstar = (Efg - Ef * Eg) / Varg;
    double VR_th = 1.0 - (Efg - Ef * Eg) * (Efg - Ef * Eg) / (Varf * Varg);
    double VR;
    if (!plain || !cv) { free(plain); free(cv); return 0; }
    mlab_rng_seed(&rng, 99);
    for (r = 0; r < R; ++r) {
        double sf = 0, sg = 0;
        for (i = 0; i < N; ++i) {
            double u = mlab_rng_uniform(&rng);
            sf += exp(u);
            sg += u;
        }
        plain[r] = sf / N;
        cv[r] = sf / N - cstar * (sg / N - Eg);
    }
    VR = mlab_var(cv, R) / mlab_var(plain, R);
    g_vr[0] = VR;
    g_vr_th[0] = VR_th;
    ok = fabs(VR - VR_th) / (VR_th > 0 ? VR_th : 1) < 0.10;
    {
        char d[96];
        snprintf(d, sizeof d, "VR_emp=%.4f theory=%.4f (<10%%)", VR, VR_th);
        test_record(ok, "control_variate", "exp_u_reduction_vs_theory", d);
    }
    free(plain); free(cv);
    return ok;
}

/* 计划批 1 A2④：control_variate 套件补第 2 case — 最优 c* 优于 c=0 与 2c* */

static int t_cv_suboptimal_c(void)
{
    const int N = 2000, R = 1500;
    mlab_rng rng;
    double *plain = malloc(sizeof(double) * (size_t)R);
    double *cv_star = malloc(sizeof(double) * (size_t)R);
    double *cv_2star = malloc(sizeof(double) * (size_t)R);
    int r, i, ok = 0;
    double Ef = exp(1.0) - 1.0, Eg = 0.5, Efg = 1.0;
    double Varg = 1.0 / 12.0;
    double cstar = (Efg - Ef * Eg) / Varg;
    double v_plain, v_star, v_2star;
    if (!plain || !cv_star || !cv_2star) {
        free(plain); free(cv_star); free(cv_2star);
        return 0;
    }
    mlab_rng_seed(&rng, 99);
    for (r = 0; r < R; ++r) {
        double sf = 0, sg = 0;
        for (i = 0; i < N; ++i) {
            double u = mlab_rng_uniform(&rng);
            sf += exp(u);
            sg += u;
        }
        plain[r] = sf / N;
        cv_star[r] = plain[r] - cstar * (sg / N - Eg);
        cv_2star[r] = plain[r] - 2.0 * cstar * (sg / N - Eg);
    }
    v_plain = mlab_var(plain, R);
    v_star = mlab_var(cv_star, R);
    v_2star = mlab_var(cv_2star, R);
    ok = v_star < 0.10 * v_plain && v_star < v_2star;
    {
        char d[112];
        snprintf(d, sizeof d, "var c*/plain=%.4f var 2c*/plain=%.4f (want c*<0.1·plain 且 c*<2c*)",
                 v_star / v_plain, v_2star / v_plain);
        test_record(ok, "control_variate", "optimal_c_beats_plain_and_2c", d);
    }
    free(plain); free(cv_star); free(cv_2star);
    return ok;
}

/* ---- 其余三种方差缩减（路线图 L130 知识点） ---- */

static int t_vr_antithetic(void)
{
    /* I=∫_0^1 e^u du；对偶变量 (u,1-u)。
       等评估预算（N 次评估）下：plain= N 个独立样本均值；antithetic = N/2 对的
       对均值。理论 VR = ((Var_f+Cov)/2)/(Var_f/2) = (Var_f+Cov)/Var_f ≈ 0.032 */
    const int N = 2000, R = 1000;
    mlab_rng rp, ra;
    double *plain = malloc(sizeof(double) * (size_t)R);
    double *anti = malloc(sizeof(double) * (size_t)R);
    int r, i, ok;
    double Varf = (exp(2.0) - 1.0) / 2.0 - (exp(1.0) - 1.0) * (exp(1.0) - 1.0);
    double Cov = exp(1.0) - (exp(1.0) - 1.0) * (exp(1.0) - 1.0);
    double VR, VR_th = (Varf + Cov) / Varf;
    if (!plain || !anti) { free(plain); free(anti); return 0; }
    mlab_rng_seed(&rp, 301);
    mlab_rng_seed(&ra, 302);
    for (r = 0; r < R; ++r) {
        double sp = 0, sa = 0;
        for (i = 0; i < N; ++i) sp += exp(mlab_rng_uniform(&rp));
        plain[r] = sp / N;
        for (i = 0; i < N / 2; ++i) {
            double u = mlab_rng_uniform(&ra);
            sa += (exp(u) + exp(1.0 - u)) / 2.0;
        }
        anti[r] = sa / (N / 2);
    }
    VR = mlab_var(anti, R) / mlab_var(plain, R);
    g_vr[1] = VR;
    g_vr_th[1] = VR_th;
    ok = VR > 0 && VR < 0.1;
    {
        char d[96];
        snprintf(d, sizeof d, "VR_emp=%.4f theory(等预算)=%.4f (<0.1)", VR, VR_th);
        test_record(ok, "variance_reduction", "antithetic_exp_reduction", d);
    }
    free(plain); free(anti);
    return ok;
}

static int t_vr_common_random_numbers(void)
{
    /* D = J(1.05) - J(1)，J(θ)=E[e^{θU}]。
       CRN：两配置共用同一 u；独立：两条独立流。CRN 方差应远小。 */
    const int N = 2000, R = 600;
    mlab_rng r1, r2;
    double *crn = malloc(sizeof(double) * (size_t)R);
    double *ind = malloc(sizeof(double) * (size_t)R);
    int r, i, ok;
    double VR;
    if (!crn || !ind) { free(crn); free(ind); return 0; }
    for (r = 0; r < R; ++r) {
        double s = 0;
        mlab_rng_seed(&r1, 1000ull + (unsigned long long)r);
        mlab_rng_seed(&r2, 5000ull + (unsigned long long)r);
        for (i = 0; i < N; ++i) {
            double u = mlab_rng_uniform(&r1);
            s += exp(1.05 * u) - exp(u);
        }
        crn[r] = s / N;
        s = 0;
        for (i = 0; i < N; ++i) {
            double u1 = mlab_rng_uniform(&r1); /* r1 已被 CRN 段消费，续流=独立于 r2 */
            double u2 = mlab_rng_uniform(&r2);
            s += exp(1.05 * u1) - exp(u2);
        }
        ind[r] = s / N;
    }
    VR = mlab_var(crn, R) / mlab_var(ind, R);
    g_vr[2] = VR;
    ok = VR > 0 && VR < 0.1;
    {
        char d[96];
        snprintf(d, sizeof d, "Var(CRN)/Var(indep)=%.4f (<0.1, 理论≈0.003)", VR);
        test_record(ok, "variance_reduction", "crn_difference_estimation", d);
    }
    free(crn); free(ind);
    return ok;
}

static int t_vr_stratified(void)
{
    /* m=10 等宽分层，比例分配。理论 VR ≈ 0.015（层内方差和/(m·Var_f)）。
       plain 基线用独立均匀流；分层流用 (j+u)/m。 */
    const int m = 10, N = 2000, R = 1000;
    mlab_rng rng;
    double *plain = malloc(sizeof(double) * (size_t)R);
    double *strat = malloc(sizeof(double) * (size_t)R);
    int r, i, j, ok;
    double VR;
    if (!plain || !strat) { free(plain); free(strat); return 0; }
    mlab_rng_seed(&rng, 303);
    for (r = 0; r < R; ++r) {
        double sp = 0, ss = 0;
        for (i = 0; i < N; ++i) sp += exp(mlab_rng_uniform(&rng));
        plain[r] = sp / N;
        for (j = 0; j < m; ++j) {
            double sj = 0;
            for (i = 0; i < N / m; ++i)
                sj += exp((j + mlab_rng_uniform(&rng)) / m);
            ss += sj / (N / m); /* 层内均值 */
        }
        strat[r] = ss / m;
    }
    VR = mlab_var(strat, R) / mlab_var(plain, R);
    g_vr[3] = VR;
    ok = VR > 0 && VR < 0.05;
    {
        char d[96];
        snprintf(d, sizeof d, "VR_emp=%.4f (m=10, 理论≈0.015, <0.05)", VR);
        test_record(ok, "variance_reduction", "stratified_exp_reduction", d);
    }
    free(plain); free(strat);
    return ok;
}

/* ---- 估计量与置信区间（MLE/CI 知识点落地） ---- */

static int t_est_mle_normal(void)
{
    mlab_rng r;
    const int n = 5000;
    double *x = malloc(sizeof(double) * (size_t)n);
    double mu, sig;
    int i, ok = 0;
    if (!x) return 0;
    mlab_rng_seed(&r, 401);
    for (i = 0; i < n; ++i) x[i] = mlab_rng_normal(&r);
    mlab_mle_normal(x, n, &mu, &sig);
    ok = fabs(mu) < 0.05 && fabs(sig - 1.0) < 0.05;
    {
        char d[80];
        snprintf(d, sizeof d, "MLE mu=%.4f sigma=%.4f (true 0, 1; sigma 用 1/n)", mu, sig);
        test_record(ok, "estimation", "mle_normal_moments", d);
    }
    free(x);
    return ok;
}

static int t_est_ci_coverage_95(void)
{
    /* 已知 σ=1，z=1.96 的 95% CI：500 次重复覆盖率应接近 0.95 */
    const int n = 50, R = 500;
    mlab_rng r;
    int rep, cov = 0, ok;
    double rate;
    mlab_rng_seed(&r, 402);
    for (rep = 0; rep < R; ++rep) {
        double xs[50], lo, hi;
        int i;
        for (i = 0; i < n; ++i) xs[i] = mlab_rng_normal(&r);
        if (mlab_ci_mean_z(xs, n, 1.0, 1.96, &lo, &hi) == 0 && lo < 0.0 && hi > 0.0)
            ++cov;
    }
    rate = (double)cov / R;
    ok = rate >= 0.90 && rate <= 0.99;
    {
        char d[80];
        snprintf(d, sizeof d, "coverage=%.3f (%d reps, 名义 0.95)", rate, R);
        test_record(ok, "estimation", "ci_mean_coverage_95", d);
    }
    return ok;
}

static int t_est_ci_width_scales(void)
{
    /* 置信区间宽度 ∝ 1/√n：width(n=100)/width(n=25) ≈ 0.5 */
    mlab_rng r;
    double w25 = 0, w100 = 0;
    int rep, ok;
    const int R = 200;
    mlab_rng_seed(&r, 403);
    for (rep = 0; rep < R; ++rep) {
        double xs[100], lo, hi;
        int i;
        for (i = 0; i < 100; ++i) xs[i] = mlab_rng_normal(&r);
        if (mlab_ci_mean_z(xs, 25, 1.0, 1.96, &lo, &hi) == 0) w25 += hi - lo;
        if (mlab_ci_mean_z(xs, 100, 1.0, 1.96, &lo, &hi) == 0) w100 += hi - lo;
    }
    w25 /= R;
    w100 /= R;
    ok = fabs(w100 / w25 - 0.5) < 0.05;
    {
        char d[96];
        snprintf(d, sizeof d, "width25=%.4f width100=%.4f ratio=%.4f (want 0.5)",
                 w25, w100, w100 / w25);
        test_record(ok, "estimation", "ci_width_scales_1_over_sqrt_n", d);
    }
    return ok;
}

/* ---- results CSV（全量运行时重算落盘） ---- */

static void write_csvs(void)
{
    FILE *fp;
    int i, j;
    /* KS 一类错误率：三种分布 */
    fp = fopen("results/ks_reject.csv", "w");
    if (fp) {
        fprintf(fp, "dist,n,reps,reject_rate\n");
        fprintf(fp, "uniform,200,1000,%.4f\n", g_ks_rate[0]);
        fprintf(fp, "exponential,200,1000,%.4f\n", g_ks_rate[1]);
        fprintf(fp, "normal,200,1000,%.4f\n", g_ks_rate[2]);
        fclose(fp);
    }
    /* CLT 直方图（等概率箱）与卡方 */
    fp = fopen("results/clt_hist.csv", "w");
    if (fp) {
        fprintf(fp, "bin,edge_lo,edge_hi,observed,expected\n");
        for (j = 0; j < CLT_BINS; ++j)
            fprintf(fp, "%d,%.6f,%.6f,%ld,%.1f\n", j, g_clt_edges[j], g_clt_edges[j + 1],
                    g_clt_obs[j], 4000.0 / CLT_BINS);
        fclose(fp);
    }
    fp = fopen("results/clt_chi2.csv", "w");
    if (fp) {
        fprintf(fp, "n_sample,n_means,bins,chi2,df,p\n");
        fprintf(fp, "100,4000,%d,%.4f,%d,%.6f\n", CLT_BINS, g_clt_chi2, CLT_BINS - 3, g_clt_p);
        fclose(fp);
    }
    /* CI 覆盖率（重放 estimation 实验） */
    fp = fopen("results/mle_ci.csv", "w");
    if (fp) {
        mlab_rng r;
        int rep, cov = 0;
        double lo, hi;
        mlab_rng_seed(&r, 402);
        for (rep = 0; rep < 500; ++rep) {
            double xs[50];
            for (i = 0; i < 50; ++i) xs[i] = mlab_rng_normal(&r);
            if (mlab_ci_mean_z(xs, 50, 1.0, 1.96, &lo, &hi) == 0 && lo < 0.0 && hi > 0.0)
                ++cov;
        }
        fprintf(fp, "n,reps,sigma,z,coverage\n");
        fprintf(fp, "50,500,1.0,1.96,%.4f\n", (double)cov / 500.0);
        fclose(fp);
    }
    /* 四种方差缩减方法 */
    fp = fopen("results/variance_reduction.csv", "w");
    if (fp) {
        fprintf(fp, "method,VR_empirical,VR_theory\n");
        fprintf(fp, "control_variate,%.6f,%.6f\n", g_vr[0], g_vr_th[0]);
        fprintf(fp, "antithetic,%.6f,%.6f\n", g_vr[1], g_vr_th[1]);
        fprintf(fp, "common_random_numbers,%.6f,na\n", g_vr[2]);
        fprintf(fp, "stratified,%.6f,na\n", g_vr[3]);
        fclose(fp);
    }
    (void)i;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"dist", "normal_cdf_landmarks", "正态 CDF 关键点", t_dist_normal_cdf_sym},
        {"dist", "normal_quantile_roundtrip", "分位数与 CDF 往返", t_dist_normal_quantile_roundtrip},
        {"dist", "expon_median_quantile", "指数分布中位数", t_dist_expon_quantile},
        {"dist", "erf_landmarks", "erf/erfc 关键点", t_dist_erf_landmarks},
        {"stats", "mean_var_small_sample", "小样本均值方差", t_stats_mean_var},
        {"stats", "sort_and_ecdf", "排序与经验分布", t_stats_sort_ecdf},
        {"gof", "ks_type1_uniform", "KS 一类错误率（1000 次，[0.03,0.07]）", t_gof_ks_type1},
        {"gof", "chi2_uniform_large_n", "卡方均匀性大样本", t_gof_chi2_uniform_bins},
        {"rng", "same_seed_reproducible", "同 seed 可复现", t_rng_reproducible_seed},
        {"rng", "uniform_open_unit_interval", "均匀落在开区间 (0,1)", t_rng_uniform_open_interval},
        {"rng", "normal_first_two_moments", "正态前两阶矩（Box-Muller）", t_rng_normal_moments},
        {"rng", "normal_polar_moments", "Marsaglia 极法矩", t_rng_normal_polar_moments},
        {"rng", "normal_inv_ks_vs_phi", "逆变换正态 KS 对账 Φ", t_rng_normal_inv_ks},
        {"rng", "poisson_moments", "Poisson 前两阶矩", t_rng_poisson_moments},
        {"rng", "bernoulli_rate", "Bernoulli 频率", t_rng_bernoulli_rate},
        {"rng", "chi2_uniform_api", "卡方均匀性 API", t_rng_chi2_uniform_api},
        {"rng", "acf_within_bounds", "自相关界", t_rng_acf_within_bounds},
        {"rng", "kind_name_labels", "四种发生器名称标签", t_rng_kind_name},
        {"rng", "seed_kind_and_u64_all_generators",
         "seed_kind/u64/异发生器可复现且序列可区分", t_rng_seed_kind_and_u64},
        {"rng", "autocorr_direct_analytic", "mlab_autocorr 解析对账", t_rng_autocorr_direct},
        {"clt", "exp_means_match_clt", "指数样本均值 CLT 矩", t_clt_moments},
        {"clt", "chi2_normal_fit_standardized", "标准化均值的等概率箱卡方拟合", t_clt_chi2_normal_fit},
        {"control_variate", "exp_u_reduction_vs_theory", "控制变量方差缩减 vs 理论(<10%)", t_cv_variance_reduction},
        {"control_variate", "optimal_c_beats_plain_and_2c",
         "最优 c* 优于 plain 与 2c*", t_cv_suboptimal_c},
        {"variance_reduction", "antithetic_exp_reduction", "对偶变量方差缩减", t_vr_antithetic},
        {"variance_reduction", "crn_difference_estimation", "公共随机数差值估计", t_vr_common_random_numbers},
        {"variance_reduction", "stratified_exp_reduction", "分层采样方差缩减", t_vr_stratified},
        {"estimation", "mle_normal_moments", "正态 MLE 矩", t_est_mle_normal},
        {"estimation", "ci_mean_coverage_95", "z 置信区间 95% 覆盖率", t_est_ci_coverage_95},
        {"estimation", "ci_width_scales_1_over_sqrt_n", "CI 宽度 ∝ 1/√n", t_est_ci_width_scales},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("A2-prob-stat", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
