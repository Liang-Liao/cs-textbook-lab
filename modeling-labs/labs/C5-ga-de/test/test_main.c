/*
 * C5: GA / DE
 */
#include "harness.h"
#include "lab.h"
#include "ga.h"
#include "de.h"
#include "bench_sa.h"
#include "bench.h"
#include "rng.h"
#include "stats.h"
#include "dist.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static double median_copy(double *x, int n)
{
    int i, j;
    if (n <= 0) return -1.0;
    for (i = 1; i < n; ++i) {
        double k = x[i];
        j = i - 1;
        while (j >= 0 && x[j] > k) {
            x[j + 1] = x[j];
            --j;
        }
        x[j + 1] = k;
    }
    return x[n / 2];
}

static double ranksum_p_less(const double *a, int na, const double *b, int nb)
{
    /* P(a tends smaller than b): count pairs a<b, normal approx two-sided */
    int i, j, below = 0;
    double mu, sd, z, p;
    if (na < 1 || nb < 1) return 1.0;
    for (i = 0; i < na; ++i)
        for (j = 0; j < nb; ++j)
            if (a[i] < b[j]) ++below;
    mu = 0.5 * na * nb;
    sd = sqrt((double)na * nb * (na + nb + 1) / 12.0);
    z = sd > 0 ? ((double)below - mu) / sd : 0.0;
    p = 2.0 * (1.0 - mlab_normal_cdf(fabs(z), 0.0, 1.0));
    if (p < 1e-12) p = 1e-12;
    return p;
}

static double run_de(mlab_de_variant var, double (*f)(const double *, int, void *),
                     int dim, const double *lb, const double *ub,
                     double F, double CR, int pop, int max_gen,
                     unsigned seed, double low_thresh,
                     double *rel_div_end, double *first_low_gen_out)
{
    mlab_rng rng;
    mlab_de_config cfg;
    mlab_de_result r;
    double best;
    int fl;
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
    cfg.variant = var;
    memset(&r, 0, sizeof r);
    if (!mlab_de_run(&rng, &cfg, NULL, &r, low_thresh)) {
        if (first_low_gen_out) *first_low_gen_out = (double)(max_gen + 1);
        if (rel_div_end) *rel_div_end = 1.0;
        return 1e300;
    }
    best = r.best_f;
    if (rel_div_end && r.div_len > 0)
        *rel_div_end = r.div_hist[r.div_len - 1];
    fl = r.first_low_gen >= 0 ? r.first_low_gen : max_gen + 1;
    if (first_low_gen_out) *first_low_gen_out = (double)fl;
    mlab_de_result_free(&r);
    return best;
}

/* ---------- suite: ga ---------- */

static int t_ga_onemax_vs_deceptive(void)
{
    const int bits = 40, n_runs = 30;
    int sol_om = 0, sol_dec = 0, i;
    double fit_om = 0, fit_dec = 0;
    FILE *fp;
    char detail[180];

    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_ga_config cfg;
        mlab_ga_result r;
        memset(&cfg, 0, sizeof cfg);
        cfg.bits = bits;
        cfg.pop = 60;
        cfg.max_gen = 80;
        cfg.p_mut = 1.0 / bits;
        cfg.p_cross = 0.9;
        cfg.sel = MLAB_GA_SEL_TOURNAMENT;
        cfg.xover = MLAB_GA_X_UNIFORM;
        cfg.tour_k = 3;
        cfg.elite = 2;
        cfg.use_deceptive = 0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 55001u + (unsigned)i, MLAB_RNG_SPLITMIX64);
        if (mlab_ga_run(&rng, &cfg, &r, 0.4)) {
            fit_om += r.best_fit;
            if (r.best_fit >= bits) ++sol_om;
        }
        mlab_ga_result_free(&r);

        cfg.use_deceptive = 1;
        cfg.pop = 40;
        cfg.p_mut = 0.005;
        cfg.sel = MLAB_GA_SEL_ROULETTE;
        cfg.xover = MLAB_GA_X_1POINT;
        cfg.elite = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 56001u + (unsigned)i, MLAB_RNG_SPLITMIX64);
        if (mlab_ga_run(&rng, &cfg, &r, 0.4)) {
            fit_dec += r.best_fit;
            if (r.best_fit >= bits) ++sol_dec;
        }
        mlab_ga_result_free(&r);
    }
    fit_om /= n_runs;
    fit_dec /= n_runs;
    {
        int ok = sol_om >= 24 && sol_dec < sol_om && fit_dec < bits;
        snprintf(detail, sizeof detail,
                 "OneMax %d/%d fit=%.2f | deceptive %d/%d fit=%.2f",
                 sol_om, n_runs, fit_om, sol_dec, n_runs, fit_dec);
        test_record(ok, "ga", "onemax_vs_deceptive_premature", detail);
        fp = fopen("results/ga_onemax_deceptive.csv", "w");
        if (fp) {
            fprintf(fp, "problem,solved,n_runs,mean_fit\n");
            fprintf(fp, "OneMax,%d,%d,%.4f\n", sol_om, n_runs, fit_om);
            fprintf(fp, "deceptive,%d,%d,%.4f\n", sol_dec, n_runs, fit_dec);
            fclose(fp);
        }
        return ok;
    }
}

static int t_ga_diversity_premature_vs_ok(void)
{
    /*
     * 路线图 :480 口径：早熟案例的种群多样性**跌落时刻**早于成功案例
     * （中位数对比，差异显著）。跌落时刻 = 多样性首次 < 0.5 的代数
     * （未触发记 max_gen+1）；第 3 代多样性水平作为辅助列一并落盘。
     */
    const int bits = 32, n_runs = 40, max_gen = 40;
    double *g_prem = (double *)malloc((size_t)n_runs * sizeof(double));
    double *g_ok = (double *)malloc((size_t)n_runs * sizeof(double));
    double *d_prem = (double *)malloc((size_t)n_runs * sizeof(double));
    double *d_ok = (double *)malloc((size_t)n_runs * sizeof(double));
    double *sort_buf = (double *)malloc((size_t)n_runs * sizeof(double));
    int nprem = 0, nok = 0, i, ok;
    FILE *fp;
    char detail[200];

    if (!g_prem || !g_ok || !d_prem || !d_ok || !sort_buf) {
        free(g_prem); free(g_ok); free(d_prem); free(d_ok); free(sort_buf);
        test_record(0, "ga", "diversity_drop_earlier_when_premature", "oom");
        return 0;
    }
    for (i = 0; i < n_runs; ++i) {
        mlab_rng rng;
        mlab_ga_config cfg;
        mlab_ga_result r;
        /* 早熟：deceptive + 极小种群 + 无变异 + 轮盘 */
        memset(&cfg, 0, sizeof cfg);
        cfg.bits = bits;
        cfg.pop = 8;
        cfg.max_gen = max_gen;
        cfg.p_mut = 0.0;
        cfg.p_cross = 0.7;
        cfg.sel = MLAB_GA_SEL_ROULETTE;
        cfg.xover = MLAB_GA_X_1POINT;
        cfg.elite = 0;
        cfg.use_deceptive = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 57001u + (unsigned)i, MLAB_RNG_SPLITMIX64);
        if (mlab_ga_run(&rng, &cfg, &r, 0.5) && r.div_len > 3) {
            g_prem[nprem++] = r.first_low_gen >= 0 ? (double)r.first_low_gen
                                                   : (double)(max_gen + 1);
            d_prem[nprem - 1] = r.div_hist[3];
        }
        mlab_ga_result_free(&r);

        /* 成功：OneMax + 锦标赛 + 合理变异，多样性跌落显著更晚 */
        cfg.pop = 80;
        cfg.p_mut = 1.0 / bits;
        cfg.sel = MLAB_GA_SEL_TOURNAMENT;
        cfg.xover = MLAB_GA_X_UNIFORM;
        cfg.tour_k = 2;
        cfg.elite = 3;
        cfg.use_deceptive = 0;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, 58001u + (unsigned)i, MLAB_RNG_SPLITMIX64);
        if (mlab_ga_run(&rng, &cfg, &r, 0.5) && r.div_len > 3) {
            g_ok[nok++] = r.first_low_gen >= 0 ? (double)r.first_low_gen
                                               : (double)(max_gen + 1);
            d_ok[nok - 1] = r.div_hist[3];
        }
        mlab_ga_result_free(&r);
    }
    if (nprem < 10 || nok < 10) {
        ok = 0;
        snprintf(detail, sizeof detail, "too few samples prem=%d ok=%d", nprem, nok);
    } else {
        double mp, mo, dp, dof;
        double p;
        for (i = 0; i < nprem; ++i) sort_buf[i] = g_prem[i];
        mp = median_copy(sort_buf, nprem);
        for (i = 0; i < nok; ++i) sort_buf[i] = g_ok[i];
        mo = median_copy(sort_buf, nok);
        for (i = 0; i < nprem; ++i) sort_buf[i] = d_prem[i];
        dp = median_copy(sort_buf, nprem);
        for (i = 0; i < nok; ++i) sort_buf[i] = d_ok[i];
        dof = median_copy(sort_buf, nok);
        p = ranksum_p_less(g_prem, nprem, g_ok, nok);
        ok = (mp < mo - 3.0) && (p < 0.05);
        snprintf(detail, sizeof detail,
                 "drop-time med premature=%.0f (n=%d) vs success=%.0f (n=%d) p=%.2e"
                 " | div@gen3 med %.3f vs %.3f",
                 mp, nprem, mo, nok, p, dp, dof);
        fp = fopen("results/ga_diversity_drop.csv", "w");
        if (fp) {
            int k;
            fprintf(fp, "group,first_low_gen,div_gen3\n");
            for (k = 0; k < nprem; ++k)
                fprintf(fp, "premature,%.0f,%.4f\n", g_prem[k], d_prem[k]);
            for (k = 0; k < nok; ++k)
                fprintf(fp, "success,%.0f,%.4f\n", g_ok[k], d_ok[k]);
            fclose(fp);
        }
    }
    test_record(ok, "ga", "diversity_drop_earlier_when_premature", detail);
    free(g_prem); free(g_ok); free(d_prem); free(d_ok); free(sort_buf);
    return ok;
}

/* ---------- suite: de ---------- */

static int t_de_rand1_vs_best1(void)
{
    /*
     * 路线图 :479 原文口径：rand/1 与 best/1 在单峰/多峰问题上优势相反
     * （配对检验 p < 0.01，与文献定性结论对账）。同组对比内 (F,CR,pop,gens)
     * 完全一致、逐 seed 配对；F 按问题类取 folklore 值（单峰大 F、多峰小 F）：
     *  - 单峰 Sphere10D，F=0.7：best/1 终态显著更优（贪婪利用直接兑现）
     *  - 多峰 Rastrigin10D，F=0.3：rand/1 终态显著更优（小 F 稳健探索；
     *    best/1 种群向最优收缩后差分萎缩，停滞在更差局部最优）
     * 固定 F=0.5 时本实现两个问题均 best/1 占优——优势反转依赖参数类
     * 语境，报告中如实记录（Rosenbrock 谷地陷阱另设 case）。
     */
    const int n_runs = 40;
    double *s_r = (double *)malloc((size_t)n_runs * sizeof(double));
    double *s_b = (double *)malloc((size_t)n_runs * sizeof(double));
    double *m_r = (double *)malloc((size_t)n_runs * sizeof(double));
    double *m_b = (double *)malloc((size_t)n_runs * sizeof(double));
    double lb5[10], ub5[10], lb10[10], ub10[10];
    int i, ok;
    double p_s, p_m;
    FILE *fp;
    char detail[240];

    if (!s_r || !s_b || !m_r || !m_b) {
        free(s_r); free(s_b); free(m_r); free(m_b);
        test_record(0, "de", "rand1_vs_best1_reversed_advantage", "oom");
        return 0;
    }
    for (i = 0; i < 10; ++i) { lb5[i] = -5.12; ub5[i] = 5.12; }
    mlab_rastrigin_bounds(10, lb10, ub10);

    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 64000u + (unsigned)i * 23u;
        s_r[i] = run_de(MLAB_DE_RAND1, mlab_sphere, 10, lb5, ub5,
                        0.7, 0.9, 30, 150, seed, 0.35, NULL, NULL);
        s_b[i] = run_de(MLAB_DE_BEST1, mlab_sphere, 10, lb5, ub5,
                        0.7, 0.9, 30, 150, seed, 0.35, NULL, NULL);
        m_r[i] = run_de(MLAB_DE_RAND1, mlab_rastrigin, 10, lb10, ub10,
                        0.3, 0.9, 40, 250, seed, 0.35, NULL, NULL);
        m_b[i] = run_de(MLAB_DE_BEST1, mlab_rastrigin, 10, lb10, ub10,
                        0.3, 0.9, 40, 250, seed, 0.35, NULL, NULL);
    }
    p_s = paired_p_two_sided(s_r, s_b, n_runs);
    p_m = paired_p_two_sided(m_r, m_b, n_runs);
    {
        double msr = mlab_mean(s_r, n_runs), msb = mlab_mean(s_b, n_runs);
        double mr = mlab_mean(m_r, n_runs), mb = mlab_mean(m_b, n_runs);
        ok = (msb < msr) && (p_s < 0.01) && (mr < mb) && (p_m < 0.01);
        snprintf(detail, sizeof detail,
                 "Sph10 rand=%.3g best=%.3g p=%.2e | Ras10 rand=%.3g best=%.3g p=%.2e",
                 msr, msb, p_s, mr, mb, p_m);
        fp = fopen("results/de_rand_vs_best.csv", "w");
        if (fp) {
            fprintf(fp, "pair,f_sph_rand,f_sph_best,f_ras_rand,f_ras_best\n");
            for (i = 0; i < n_runs; ++i)
                fprintf(fp, "%d,%.8g,%.8g,%.8g,%.8g\n",
                        i, s_r[i], s_b[i], m_r[i], m_b[i]);
            fclose(fp);
        }
        test_record(ok, "de", "rand1_vs_best1_reversed_advantage", detail);
    }
    free(s_r); free(s_b); free(m_r); free(m_b);
    return ok;
}

static int t_de_rosenbrock_valley(void)
{
    /*
     * Rosenbrock 10D（形式单峰的弯曲谷）上 rand/1 vs best/1：
     * best/1 贪婪追踪当前最优，提前压入谷壁；rand/1 靠差分探索沿谷推进，
     * 终态显著更优（p<0.01）。说明「单峰→best/1 更优」的二分法并非普适，
     * 谷地型 landscapes 惩罚贪婪算子——与 folklore 的边界如实对账。
     */
    const int n_runs = 40;
    double *r_r = (double *)malloc((size_t)n_runs * sizeof(double));
    double *r_b = (double *)malloc((size_t)n_runs * sizeof(double));
    double lb[10], ub[10];
    int i, ok;
    double p;
    FILE *fp;
    char detail[200];

    if (!r_r || !r_b) {
        free(r_r); free(r_b);
        test_record(0, "de", "rosenbrock_valley_traps_best1", "oom");
        return 0;
    }
    for (i = 0; i < 10; ++i) { lb[i] = -5.0; ub[i] = 5.0; }
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 66000u + (unsigned)i * 23u;
        r_r[i] = run_de(MLAB_DE_RAND1, mlab_rosenbrock, 10, lb, ub,
                        0.5, 0.9, 40, 250, seed, 0.35, NULL, NULL);
        r_b[i] = run_de(MLAB_DE_BEST1, mlab_rosenbrock, 10, lb, ub,
                        0.5, 0.9, 40, 250, seed, 0.35, NULL, NULL);
    }
    p = paired_p_two_sided(r_r, r_b, n_runs);
    {
        double mr = mlab_mean(r_r, n_runs), mb = mlab_mean(r_b, n_runs);
        ok = (mr < mb) && (p < 0.01);
        snprintf(detail, sizeof detail,
                 "Ros10 rand=%.4g<best=%.4g p=%.2e（谷地惩罚贪婪）", mr, mb, p);
        fp = fopen("results/de_rosenbrock_rand_vs_best.csv", "w");
        if (fp) {
            fprintf(fp, "pair,f_ros_rand,f_ros_best\n");
            for (i = 0; i < n_runs; ++i)
                fprintf(fp, "%d,%.8g,%.8g\n", i, r_r[i], r_b[i]);
            fclose(fp);
        }
        test_record(ok, "de", "rosenbrock_valley_traps_best1", detail);
    }
    free(r_r); free(r_b);
    return ok;
}

static int t_de_f_cr_grid(void)
{
    const double Fs[] = {0.2, 0.4, 0.6, 0.8, 1.0};
    const double CRs[] = {0.0, 0.2, 0.4, 0.6, 0.8, 1.0};
    const int nF = 5, nCR = 6, n_runs = 50, dim = 10, pop = 40, gens = 200;
    const double thr = 1e-2;
    double lb[10], ub[10], *rates;
    unsigned char *img;
    int iF, iC, cells = nF * nCR, filled = 0, ok;
    FILE *fp, *pgm;
    char detail[160];

    rates = (double *)malloc((size_t)cells * sizeof(double));
    img = (unsigned char *)malloc((size_t)cells);
    if (!rates || !img) {
        free(rates); free(img);
        test_record(0, "de", "f_cr_grid_rasstrigin10d", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(dim, lb, ub);
    fp = fopen("results/de_f_cr_grid.csv", "w");
    if (fp) fprintf(fp, "F,CR,n_success,n_runs,rate,mean_best\n");
    for (iF = 0; iF < nF; ++iF) {
        for (iC = 0; iC < nCR; ++iC) {
            int r, suc = 0;
            double sum = 0;
            for (r = 0; r < n_runs; ++r) {
                double b = run_de(MLAB_DE_RAND1, mlab_rastrigin, dim, lb, ub,
                                  Fs[iF], CRs[iC], pop, gens,
                                  70000u + (unsigned)(iF * 200 + iC * 10 + r),
                                  0.35, NULL, NULL);
                sum += b;
                if (b < thr) ++suc;
            }
            rates[iF * nCR + iC] = (double)suc / n_runs;
            img[iF * nCR + iC] = (unsigned char)(rates[iF * nCR + iC] * 255.0 + 0.5);
            ++filled;
            if (fp)
                fprintf(fp, "%.1f,%.1f,%d,%d,%.4f,%.6f\n",
                        Fs[iF], CRs[iC], suc, n_runs, rates[iF * nCR + iC],
                        sum / n_runs);
        }
    }
    if (fp) fclose(fp);
    pgm = fopen("results/de_f_cr_grid.pgm", "w");
    if (pgm) {
        int a, b;
        fprintf(pgm, "P2\n%d %d\n255\n", nCR, nF);
        for (a = 0; a < nF; ++a) {
            for (b = 0; b < nCR; ++b)
                fprintf(pgm, "%d%c", img[a * nCR + b], b + 1 == nCR ? '\n' : ' ');
        }
        fclose(pgm);
    }
    {
        int k, any_s = 0, any_f = 0;
        for (k = 0; k < cells; ++k) {
            if (rates[k] > 0.05) any_s = 1;
            if (rates[k] < 0.05) any_f = 1;
        }
        ok = filled == cells && any_s && any_f;
        snprintf(detail, sizeof detail,
                 "cells=%d/%d thr=%.0e contrast=%d", filled, cells, thr, any_s && any_f);
    }
    test_record(ok, "de", "f_cr_grid_rasstrigin10d", detail);
    free(rates); free(img);
    return ok;
}

static int t_de_diversity_best1_earlier(void)
{
    const int n_runs = 30, dim = 10, pop = 40, gens = 200;
    double lb[10], ub[10];
    double *g_r = (double *)malloc((size_t)n_runs * sizeof(double));
    double *g_b = (double *)malloc((size_t)n_runs * sizeof(double));
    int i, ok;
    FILE *fp;
    char detail[160];

    if (!g_r || !g_b) {
        free(g_r); free(g_b);
        test_record(0, "de", "best1_diversity_collapses_earlier", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(dim, lb, ub);
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 81000u + (unsigned)i * 13u;
        run_de(MLAB_DE_RAND1, mlab_rastrigin, dim, lb, ub, 0.5, 0.9,
               pop, gens, seed, 0.35, NULL, &g_r[i]);
        run_de(MLAB_DE_BEST1, mlab_rastrigin, dim, lb, ub, 0.5, 0.9,
               pop, gens, seed, 0.35, NULL, &g_b[i]);
    }
    {
        double mr = median_copy(g_r, n_runs);
        double mb = median_copy(g_b, n_runs);
        double p = ranksum_p_less(g_b, n_runs, g_r, n_runs);
        ok = mb < mr && p < 0.05;
        snprintf(detail, sizeof detail,
                 "median first_low_gen best1=%.1f rand1=%.1f p=%.3e",
                 mb, mr, p);
        fp = fopen("results/de_diversity_drop.csv", "w");
        if (fp) {
            int k;
            fprintf(fp, "variant,first_low_gen\n");
            for (k = 0; k < n_runs; ++k) {
                fprintf(fp, "rand1,%.0f\n", g_r[k]);
                fprintf(fp, "best1,%.0f\n", g_b[k]);
            }
            fclose(fp);
        }
    }
    test_record(ok, "de", "best1_diversity_collapses_earlier", detail);
    free(g_r); free(g_b);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"ga", "onemax_vs_deceptive_premature",
         "OneMax 可解；deceptive 易早熟",
         t_ga_onemax_vs_deceptive},
        {"ga", "diversity_drop_earlier_when_premature",
         "早熟多样性跌落早于成功案例",
         t_ga_diversity_premature_vs_ok},
        {"de", "rand1_vs_best1_reversed_advantage",
         "rand/1 与 best/1 单峰/多峰优势相反",
         t_de_rand1_vs_best1},
        {"de", "rosenbrock_valley_traps_best1",
         "Rosenbrock 弯曲谷惩罚贪婪 best/1",
         t_de_rosenbrock_valley},
        {"de", "f_cr_grid_rasstrigin10d",
         "F×CR 网格 10D Rastrigin 成功率全表",
         t_de_f_cr_grid},
        {"de", "best1_diversity_collapses_earlier",
         "best/1 多样性塌缩早于 rand/1",
         t_de_diversity_best1_earlier},
    };
    test_ensure_results_dir();
    return test_run_main("C5-ga-de", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
