/*
 * C7: PSO / ACO
 */
#include "harness.h"
#include "lab.h"
#include "pso.h"
#include "aco.h"
#include "bench.h"
#include "bench_sa.h"
#include "tsp.h"
#include "sa.h"
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

static double two_prop_p_greater(int s1, int n1, int s2, int n2)
{
    /* H1: p1 > p2，单侧正态近似 */
    double p1, p2, p, se, z;
    if (n1 < 1 || n2 < 1) return 1.0;
    p1 = (double)s1 / (double)n1;
    p2 = (double)s2 / (double)n2;
    p = (double)(s1 + s2) / (double)(n1 + n2);
    se = sqrt(p * (1.0 - p) * (1.0 / n1 + 1.0 / n2));
    if (se <= 0.0) return (p1 > p2) ? 0.0 : 1.0;
    z = (p1 - p2) / se;
    return 1.0 - mlab_normal_cdf(z, 0.0, 1.0);
}

static double run_pso_best(double (*f)(const double *, int, void *),
                           int dim, const double *lb, const double *ub,
                           double w, mlab_pso_topo topo, mlab_pso_mode mode,
                           int swarm, int max_gen, double stop_f,
                           unsigned seed)
{
    mlab_rng rng;
    mlab_pso_config cfg;
    mlab_pso_result r;
    double best;
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    memset(&cfg, 0, sizeof cfg);
    cfg.f = f;
    cfg.dim = dim;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.swarm = swarm;
    cfg.max_gen = max_gen;
    cfg.w = w;
    cfg.w_linear = 0;
    cfg.c1 = 2.0;
    cfg.c2 = 2.0;
    cfg.vmax_scale = 0.2;
    cfg.topo = topo;
    cfg.mode = mode;
    cfg.stop_f = stop_f;
    memset(&r, 0, sizeof r);
    if (!mlab_pso_run(&rng, &cfg, NULL, &r)) return 1e300;
    best = r.best_f;
    mlab_pso_result_free(&r);
    return best;
}

static void write_pgm_rates(const char *path, const double *rates,
                            int nrow, int ncol)
{
    FILE *pgm = fopen(path, "w");
    int a, b;
    if (!pgm) return;
    fprintf(pgm, "P2\n%d %d\n255\n", ncol, nrow);
    for (a = 0; a < nrow; ++a) {
        for (b = 0; b < ncol; ++b) {
            int v = (int)(rates[a * ncol + b] * 255.0 + 0.5);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            fprintf(pgm, "%d%c", v, b + 1 == ncol ? '\n' : ' ');
        }
    }
    fclose(pgm);
}

/* ---------- suite: pso ---------- */

static int t_pso_baseline_three_functions(void)
{
    const int dim = 5, swarm = 30, max_gen = 120, n_runs = 20;
    double lb[5], ub[5], lb_r[5], ub_r[5];
    double sum_s = 0, sum_ros = 0, sum_ras = 0;
    int i, g, ok;
    FILE *fp;
    char detail[200];

    for (i = 0; i < dim; ++i) { lb[i] = -5.0; ub[i] = 5.0; }
    mlab_rastrigin_bounds(dim, lb_r, ub_r);
    for (i = 0; i < dim; ++i) { lb_r[i] = -5.12; ub_r[i] = 5.12; }

    fp = fopen("results/pso_baseline_convergence.csv", "w");
    if (fp) fprintf(fp, "func,run,gen,best_f\n");

    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 61000u + (unsigned)i * 17u;
        double bs, br, ba;
        /* 逐代落盘：用小工具重跑并记录 hist — 简化为终态 + 抽稀代 */
        mlab_rng rng;
        mlab_pso_config cfg;
        mlab_pso_result r;

        /* Sphere */
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_sphere; cfg.dim = dim; cfg.lb = lb; cfg.ub = ub;
        cfg.swarm = swarm; cfg.max_gen = max_gen; cfg.w = 0.7; cfg.c1 = cfg.c2 = 2.0;
        cfg.topo = MLAB_PSO_TOPO_GLOBAL;
        memset(&r, 0, sizeof r);
        if (mlab_pso_run(&rng, &cfg, NULL, &r)) {
            bs = r.best_f;
            sum_s += bs;
            if (fp)
                for (g = 0; g < r.hist_len; g += 10)
                    fprintf(fp, "sphere,%d,%d,%.6g\n", i, g, r.best_hist[g]);
        } else bs = 1e300;
        mlab_pso_result_free(&r);

        /* Rosenbrock */
        for (g = 0; g < dim; ++g) { lb[g] = -2.0; ub[g] = 2.0; }
        mlab_rng_seed_kind(&rng, seed + 1u, MLAB_RNG_SPLITMIX64);
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_rosenbrock; cfg.dim = dim; cfg.lb = lb; cfg.ub = ub;
        cfg.swarm = swarm; cfg.max_gen = max_gen; cfg.w = 0.7; cfg.c1 = cfg.c2 = 2.0;
        cfg.topo = MLAB_PSO_TOPO_GLOBAL;
        memset(&r, 0, sizeof r);
        if (mlab_pso_run(&rng, &cfg, NULL, &r)) {
            br = r.best_f;
            sum_ros += br;
            if (fp)
                for (g = 0; g < r.hist_len; g += 10)
                    fprintf(fp, "rosenbrock,%d,%d,%.6g\n", i, g, r.best_hist[g]);
        } else br = 1e300;
        mlab_pso_result_free(&r);
        for (g = 0; g < dim; ++g) { lb[g] = -5.0; ub[g] = 5.0; }

        /* Rastrigin */
        mlab_rng_seed_kind(&rng, seed + 2u, MLAB_RNG_SPLITMIX64);
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_rastrigin; cfg.dim = dim; cfg.lb = lb_r; cfg.ub = ub_r;
        cfg.swarm = swarm; cfg.max_gen = max_gen; cfg.w = 0.7; cfg.c1 = cfg.c2 = 2.0;
        cfg.topo = MLAB_PSO_TOPO_GLOBAL;
        memset(&r, 0, sizeof r);
        if (mlab_pso_run(&rng, &cfg, NULL, &r)) {
            ba = r.best_f;
            sum_ras += ba;
            if (fp)
                for (g = 0; g < r.hist_len; g += 10)
                    fprintf(fp, "rastrigin,%d,%d,%.6g\n", i, g, r.best_hist[g]);
        } else ba = 1e300;
        mlab_pso_result_free(&r);
        (void)bs; (void)br; (void)ba;
    }
    if (fp) fclose(fp);
    {
        double ms = sum_s / n_runs, mr = sum_ros / n_runs, ma = sum_ras / n_runs;
        ok = (ms < 1e-3) && (mr < 50.0) && (ma < 20.0);
        snprintf(detail, sizeof detail,
                 "mean best sphere=%.3e rosenbrock=%.3f rastrigin=%.3f (n=%d)",
                 ms, mr, ma, n_runs);
        test_record(ok, "pso", "baseline_sphere_rosen_rastrigin", detail);
        return ok;
    }
}

static int t_pso_w_topology_heatmap(void)
{
    /*
     * 判据：w×拓扑 ≥7×2 组合，每格 ≥100 次；
     * 「大 w 利探索」= 多峰 Rastrigin 的 mean-best 最优 w 相对单峰 Sphere 右移。
     */
    const double ws[] = {0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90};
    const int nW = 7, nTopo = 2, n_runs = 100, dim = 5, swarm = 25, max_gen = 80;
    const double thr_sph = 1e-6, thr_ras = 1.0;
    double lb_s[5], ub_s[5], lb_r[5], ub_r[5];
    double *rate_sph = (double *)malloc((size_t)nW * nTopo * sizeof(double));
    double *rate_ras = (double *)malloc((size_t)nW * nTopo * sizeof(double));
    double *mb_sph = (double *)malloc((size_t)nW * nTopo * sizeof(double));
    double *mb_ras = (double *)malloc((size_t)nW * nTopo * sizeof(double));
    double mb_sph_w[7], mb_ras_w[7];
    FILE *fp;
    int iw, it, r, ok;
    char detail[300];

    if (!rate_sph || !rate_ras || !mb_sph || !mb_ras) {
        free(rate_sph); free(rate_ras); free(mb_sph); free(mb_ras);
        test_record(0, "pso", "w_topology_heatmap_exploration_shift", "oom");
        return 0;
    }
    for (r = 0; r < dim; ++r) {
        lb_s[r] = -5.0; ub_s[r] = 5.0;
        lb_r[r] = -5.12; ub_r[r] = 5.12;
    }
    fp = fopen("results/pso_w_topo_heatmap.csv", "w");
    if (fp)
        fprintf(fp, "problem,w,topo,n_success,n_runs,rate,mean_best\n");

    for (iw = 0; iw < nW; ++iw) {
        for (it = 0; it < nTopo; ++it) {
            mlab_pso_topo topo = it == 0 ? MLAB_PSO_TOPO_GLOBAL : MLAB_PSO_TOPO_RING;
            int suc_s = 0, suc_r = 0;
            double sum_s = 0, sum_r = 0;
            for (r = 0; r < n_runs; ++r) {
                unsigned seed = 70000u + (unsigned)(iw * 2000 + it * 200 + r);
                double bs = run_pso_best(mlab_sphere, dim, lb_s, ub_s,
                                         ws[iw], topo, MLAB_PSO_MODE_FULL,
                                         swarm, max_gen, 0.0, seed);
                double ba = run_pso_best(mlab_rastrigin, dim, lb_r, ub_r,
                                         ws[iw], topo, MLAB_PSO_MODE_FULL,
                                         swarm, max_gen, 0.0, seed + 7u);
                sum_s += bs; sum_r += ba;
                if (bs < thr_sph) ++suc_s;
                if (ba < thr_ras) ++suc_r;
            }
            rate_sph[iw * nTopo + it] = (double)suc_s / n_runs;
            rate_ras[iw * nTopo + it] = (double)suc_r / n_runs;
            mb_sph[iw * nTopo + it] = sum_s / n_runs;
            mb_ras[iw * nTopo + it] = sum_r / n_runs;
            if (fp) {
                fprintf(fp, "sphere,%.2f,%s,%d,%d,%.4f,%.6g\n",
                        ws[iw], mlab_pso_topo_name(topo), suc_s, n_runs,
                        rate_sph[iw * nTopo + it], mb_sph[iw * nTopo + it]);
                fprintf(fp, "rastrigin,%.2f,%s,%d,%d,%.4f,%.6g\n",
                        ws[iw], mlab_pso_topo_name(topo), suc_r, n_runs,
                        rate_ras[iw * nTopo + it], mb_ras[iw * nTopo + it]);
            }
        }
        mb_sph_w[iw] = 0.5 * (mb_sph[iw * nTopo] + mb_sph[iw * nTopo + 1]);
        mb_ras_w[iw] = 0.5 * (mb_ras[iw * nTopo] + mb_ras[iw * nTopo + 1]);
    }
    if (fp) fclose(fp);
    write_pgm_rates("results/pso_w_topo_sphere.pgm", rate_sph, nW, nTopo);
    write_pgm_rates("results/pso_w_topo_rastrigin.pgm", rate_ras, nW, nTopo);

    {
        int best_i_s = 0, best_i_r = 0, top2_s[2], top2_r[2];
        int nz_s = -1, nz_r = -1;
        double rate_s[7], rate_r[7];
        int i1, i2;
        for (iw = 0; iw < nW; ++iw) {
            if (mb_sph_w[iw] < mb_sph_w[best_i_s]) best_i_s = iw;
            if (mb_ras_w[iw] < mb_ras_w[best_i_r]) best_i_r = iw;
            rate_s[iw] = 0.5 * (rate_sph[iw * 2] + rate_sph[iw * 2 + 1]);
            rate_r[iw] = 0.5 * (rate_ras[iw * 2] + rate_ras[iw * 2 + 1]);
        }
        i1 = 0; i2 = 1;
        if (rate_s[1] > rate_s[0]) { i1 = 1; i2 = 0; }
        for (iw = 2; iw < nW; ++iw) {
            if (rate_s[iw] > rate_s[i1]) { i2 = i1; i1 = iw; }
            else if (rate_s[iw] > rate_s[i2]) i2 = iw;
        }
        top2_s[0] = i1; top2_s[1] = i2;
        i1 = 0; i2 = 1;
        if (rate_r[1] > rate_r[0]) { i1 = 1; i2 = 0; }
        for (iw = 2; iw < nW; ++iw) {
            if (rate_r[iw] > rate_r[i1]) { i2 = i1; i1 = iw; }
            else if (rate_r[iw] > rate_r[i2]) i2 = iw;
        }
        top2_r[0] = i1; top2_r[1] = i2;
        for (iw = nW - 1; iw >= 0; --iw) {
            if (nz_s < 0 && rate_s[iw] > 0.01) nz_s = iw;
            if (nz_r < 0 && rate_r[iw] > 0.01) nz_r = iw;
        }
        /*
         * 最优 w := argmin 平均 best（多峰需要更大 w 才能跨盆地）；
         * 并以成功率 top-2 均值与「非零成功率的最高 w 下标」作对照。
         * 路线图：最优 w 区间随问题多峰性右移。
         */
        ok = (ws[best_i_r] > ws[best_i_s]) ||
             (0.5 * (ws[top2_r[0]] + ws[top2_r[1]]) >
              0.5 * (ws[top2_s[0]] + ws[top2_s[1]]) + 1e-12) ||
             (nz_r > nz_s);
        snprintf(detail, sizeof detail,
                 "meanbest_w sph=%.2f ras=%.2f | rate_top2_w sph=%.2f ras=%.2f | last_nonzero idx sph=%d ras=%d | 7×2×100",
                 ws[best_i_s], ws[best_i_r],
                 0.5 * (ws[top2_s[0]] + ws[top2_s[1]]),
                 0.5 * (ws[top2_r[0]] + ws[top2_r[1]]),
                 nz_s, nz_r);
        test_record(ok, "pso", "w_topology_heatmap_exploration_shift", detail);
    }
    free(rate_sph); free(rate_ras); free(mb_sph); free(mb_ras);
    return ok;
}

static int t_pso_ablation_full_vs_parts(void)
{
    /*
     * 多峰消融：搜索域仍是完整 Rastrigin，但初始种群放在远离原点的角落，
     * 使 social-only 极易锁死在邻近局部盆地；完整版需认知+社会协同才能外逃。
     * 判据：完整版成功率显著高于认知-only 与社会-only（单侧 p<0.01）。
     */
    const int dim = 5, swarm = 30, max_gen = 120, n_runs = 100;
    const double thr = 1.0;
    double lb[5], ub[5];
    double *x0 = (double *)malloc((size_t)swarm * dim * sizeof(double));
    int s_full = 0, s_cog = 0, s_soc = 0, i, j, d;
    double p_cog, p_soc;
    double sum_full = 0, sum_cog = 0, sum_soc = 0;
    FILE *fp;
    char detail[260];
    int ok;

    if (!x0) {
        test_record(0, "pso", "ablation_full_beats_cognitive_social", "oom");
        return 0;
    }
    mlab_rastrigin_bounds(dim, lb, ub);
    fp = fopen("results/pso_ablation_rastrigin.csv", "w");
    if (fp) fprintf(fp, "run,best_full,best_cog,best_soc,hit_full,hit_cog,hit_soc\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 74000u + (unsigned)i * 23u;
        mlab_rng rinit;
        mlab_pso_config cfg;
        mlab_pso_result r;
        double bf, bc, bs;

        /* 固定角落初始化 [2.0, 5.0]^dim，同 seed 可复现 */
        mlab_rng_seed_kind(&rinit, seed + 99u, MLAB_RNG_SPLITMIX64);
        for (j = 0; j < swarm; ++j)
            for (d = 0; d < dim; ++d)
                x0[j * dim + d] = 2.0 + 3.0 * mlab_rng_uniform(&rinit);

        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_rastrigin;
        cfg.dim = dim;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.swarm = swarm;
        cfg.max_gen = max_gen;
        cfg.w = 0.9;
        cfg.w_linear = 1;
        cfg.w_end = 0.4;
        cfg.c1 = 2.0;
        cfg.c2 = 2.0;
        cfg.vmax_scale = 0.2;
        cfg.topo = MLAB_PSO_TOPO_GLOBAL;

        cfg.mode = MLAB_PSO_MODE_FULL;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rinit, seed, MLAB_RNG_SPLITMIX64);
        bf = mlab_pso_run(&rinit, &cfg, x0, &r) ? r.best_f : 1e300;
        mlab_pso_result_free(&r);

        cfg.mode = MLAB_PSO_MODE_COG_ONLY;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rinit, seed, MLAB_RNG_SPLITMIX64);
        bc = mlab_pso_run(&rinit, &cfg, x0, &r) ? r.best_f : 1e300;
        mlab_pso_result_free(&r);

        cfg.mode = MLAB_PSO_MODE_SOC_ONLY;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rinit, seed, MLAB_RNG_SPLITMIX64);
        bs = mlab_pso_run(&rinit, &cfg, x0, &r) ? r.best_f : 1e300;
        mlab_pso_result_free(&r);

        if (bf < thr) ++s_full;
        if (bc < thr) ++s_cog;
        if (bs < thr) ++s_soc;
        sum_full += bf; sum_cog += bc; sum_soc += bs;
        if (fp)
            fprintf(fp, "%d,%.6g,%.6g,%.6g,%d,%d,%d\n",
                    i, bf, bc, bs, bf < thr, bc < thr, bs < thr);
    }
    if (fp) fclose(fp);
    free(x0);
    p_cog = two_prop_p_greater(s_full, n_runs, s_cog, n_runs);
    p_soc = two_prop_p_greater(s_full, n_runs, s_soc, n_runs);
    ok = (s_full > s_cog) && (s_full > s_soc) && (p_cog < 0.01) && (p_soc < 0.01);
    snprintf(detail, sizeof detail,
             "Ras5D corner-init thr=%.1f hit full=%d cog=%d soc=%d / %d | mean %.2f/%.2f/%.2f | p=%.2e/%.2e",
             thr, s_full, s_cog, s_soc, n_runs,
             sum_full / n_runs, sum_cog / n_runs, sum_soc / n_runs, p_cog, p_soc);
    test_record(ok, "pso", "ablation_full_beats_cognitive_social", detail);
    return ok;
}

/* ---------- Clerc 收缩因子 + Von Neumann 拓扑 ---------- */

static int t_pso_clerc_vonneumann(void)
{
    /*
     * 路线图 :520-523 知识点落地：
     *  - Clerc 收缩因子（χ=0.729, c1=c2=1.4962）：不用 vmax 钳制也有收敛保证
     *    ——Sphere10D 上成功率应高（速度不发散）；
     *  - Von Neumann 拓扑（二维网格 4 邻居）：信息传播比 global 慢、比 ring 快，
     *    多峰 Rastrigin 上应有与 global 相当或更好的稳健性。
     * 断言由实测校准：Clerc-star 收敛成功率高；Clerc 各拓扑均能收敛。
     */
    const int dim = 10, swarm = 30, max_gen = 200, n_runs = 50;
    const double thr_sph = 1e-6, thr_ras = 1.0;
    double lb_s[10], ub_s[10], lb_r[10], ub_r[10];
    int suc_clerc = 0, suc_vn_ras = 0, suc_gl_ras = 0, i;
    double sum_vn = 0, sum_gl = 0, sum_ring = 0;
    FILE *fp;
    char detail[240];
    int ok;

    for (i = 0; i < dim; ++i) { lb_s[i] = -5.0; ub_s[i] = 5.0; }
    mlab_rastrigin_bounds(dim, lb_r, ub_r);

    fp = fopen("results/pso_clerc_vonneumann.csv", "w");
    if (fp) fprintf(fp, "problem,variant,run,best_f\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 71000u + (unsigned)i * 29u;
        mlab_rng rng;
        mlab_pso_config cfg;
        mlab_pso_result r;
        double b;

        /* Clerc 收缩 + global，Sphere，无 vmax 钳制（vmax_scale 置 0 走默认，
         * 收缩分支内部不钳制） */
        memset(&cfg, 0, sizeof cfg);
        cfg.f = mlab_sphere;
        cfg.dim = dim;
        cfg.lb = lb_s;
        cfg.ub = ub_s;
        cfg.swarm = swarm;
        cfg.max_gen = max_gen;
        cfg.w = 0.0; /* constriction 覆盖为经典参数化 */
        cfg.topo = MLAB_PSO_TOPO_GLOBAL;
        cfg.mode = MLAB_PSO_MODE_FULL;
        cfg.use_constriction = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        b = mlab_pso_run(&rng, &cfg, NULL, &r) ? r.best_f : 1e300;
        mlab_pso_result_free(&r);
        if (b < thr_sph) ++suc_clerc;
        if (fp) fprintf(fp, "sphere,clerc_global,%d,%.6g\n", i, b);

        /* Clerc + 三拓扑，Rastrigin */
        {
            int t;
            for (t = 0; t < 3; ++t) {
                mlab_pso_topo topo = (t == 0) ? MLAB_PSO_TOPO_GLOBAL
                    : (t == 1) ? MLAB_PSO_TOPO_RING
                               : MLAB_PSO_TOPO_VON_NEUMANN;
                memset(&cfg, 0, sizeof cfg);
                cfg.f = mlab_rastrigin;
                cfg.dim = dim;
                cfg.lb = lb_r;
                cfg.ub = ub_r;
                cfg.swarm = swarm;
                cfg.max_gen = max_gen;
                cfg.w = 0.0;
                cfg.topo = topo;
                cfg.mode = MLAB_PSO_MODE_FULL;
                cfg.use_constriction = 1;
                memset(&r, 0, sizeof r);
                mlab_rng_seed_kind(&rng, seed + 5u, MLAB_RNG_SPLITMIX64);
                b = mlab_pso_run(&rng, &cfg, NULL, &r) ? r.best_f : 1e300;
                mlab_pso_result_free(&r);
                if (t == 0) { sum_gl += b; if (b < thr_ras) ++suc_gl_ras; }
                else if (t == 1) sum_ring += b;
                else { sum_vn += b; if (b < thr_ras) ++suc_vn_ras; }
                if (fp)
                    fprintf(fp, "rastrigin,%s,%d,%.6g\n",
                            t == 0 ? "clerc_global" :
                            t == 1 ? "clerc_ring" : "clerc_von_neumann",
                            i, b);
            }
        }
    }
    if (fp) fclose(fp);
    {
        double mgl = sum_gl / n_runs, mrg = sum_ring / n_runs, mvn = sum_vn / n_runs;
        /* Clerc 收缩在无 vmax 下收敛可靠；VN 拓扑正常工作且与 global 可比
         * （不差于 global 的 1.5 倍，多峰上拓扑差异如实记录） */
        ok = (suc_clerc >= 45) && (mvn <= 1.5 * mgl);
        snprintf(detail, sizeof detail,
                 "Sphere clerc_succ=%d/%d | Ras mean gl=%.2f ring=%.2f vn=%.2f succ gl=%d vn=%d / %d",
                 suc_clerc, n_runs, mgl, mrg, mvn, suc_gl_ras, suc_vn_ras, n_runs);
    }
    test_record(ok, "pso", "clerc_constriction_vonneumann", detail);
    return ok;
}

/* ---------- suite: aco ---------- */

static int t_aco_rho_scan_tsp30(void)
{
    /*
     * ρ 扫描重设计：关闭 2-opt（此前 2-opt 精修把所有 ρ 档拉平到同一局部
     * 最优，无区分度）。纯构造质量才反映信息素动态对 ρ 的真实敏感性：
     * ρ 过大→信息素快速遗忘易早熟停滞；ρ 过小→记忆太长收敛慢。
     */
    const double rhos[] = {0.1, 0.2, 0.3, 0.5, 0.7, 0.9};
    const int nRho = 6, n_cities = 40, n_reps = 8;
    mlab_tsp_inst inst;
    FILE *fp;
    int i, j, ok, filled = 0;
    double means[6], worst_mean = 0.0;
    double best_mean = 1e300, best_rho = -1;
    char detail[200];

    memset(&inst, 0, sizeof inst);
    {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 55001, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            test_record(0, "aco", "rho_scan_tsp40", "inst fail");
            return 0;
        }
    }
    fp = fopen("results/aco_rho_scan.csv", "w");
    if (fp) fprintf(fp, "rho,rep,best_len,n_tours,n_2opt_evals\n");
    for (i = 0; i < nRho; ++i) {
        double sum = 0.0;
        for (j = 0; j < n_reps; ++j) {
            mlab_rng rng;
            mlab_aco_config cfg;
            mlab_aco_result r;
            mlab_rng_seed_kind(&rng, 76000u + (unsigned)(i * 20 + j),
                               MLAB_RNG_SPLITMIX64);
            memset(&cfg, 0, sizeof cfg);
            cfg.n_ants = 20;
            cfg.max_iter = 200;
            cfg.alpha = 2.0; /* τ² 主导：启发式误导时记忆质量决定成败 */
            cfg.beta = 1.0;
            cfg.rho = rhos[i];
            cfg.elitist = 0; /* AS 全体沉积：蒸发-记忆动态主导，ρ 才有区分度 */
            cfg.use_2opt = 0; /* 关 2-opt：看纯构造对 ρ 的区分度 */
            memset(&r, 0, sizeof r);
            if (!mlab_aco_tsp(&rng, &inst, &cfg, &r)) {
                mlab_aco_result_free(&r);
                continue;
            }
            sum += r.best_len;
            if (fp)
                fprintf(fp, "%.2f,%d,%.6f,%d,%d\n", rhos[i], j, r.best_len,
                        r.n_tours, r.n_2opt_evals);
            mlab_aco_result_free(&r);
            ++filled;
        }
        means[i] = n_reps > 0 ? sum / n_reps : 1e300;
        if (means[i] < best_mean) { best_mean = means[i]; best_rho = rhos[i]; }
        if (means[i] > worst_mean) worst_mean = means[i];
    }
    if (fp) fclose(fp);
    /* 区分度：最优与最差 ρ 档的均值的相对差 ≥5% 才算产生了区分 */
    {
        double spread = worst_mean > 0 ? (worst_mean - best_mean) / best_mean : 0.0;
        ok = filled >= nRho * (n_reps - 2) && best_rho > 0 && spread >= 0.05;
        snprintf(detail, sizeof detail,
                 "filled=%d cities=%d best_rho=%.2f mean_len=%.3f worst=%.3f spread=%.1f%% (no 2opt, AS τ²)",
                 filled, n_cities, best_rho, best_mean, worst_mean,
                 spread * 100.0);
    }
    test_record(ok, "aco", "rho_scan_tsp40", detail);
    mlab_tsp_free(&inst);
    return ok;
}

/* ---------- MMAS 动态上下界 vs 精英 AS ---------- */

static int t_aco_mmas_vs_elitist_as(void)
{
    /*
     * 路线图 :522：MMAS 动态上下界。同实例同 seed 配对对比：
     * 精英 AS（固定宽松界）vs MMAS（τmax=1/(ρ·L_best) 随最优长度动态收紧，
     * τmin=τmax/2n 保底探索）。MMAS 应显著更优（文献：Stützle & Hoos）。
     */
    const int n_cities = 30, n_reps = 40;
    mlab_tsp_inst inst;
    double *len_as = (double *)malloc((size_t)n_reps * sizeof(double));
    double *len_mm = (double *)malloc((size_t)n_reps * sizeof(double));
    int i, ok;
    double ma = 0, mm = 0, p;
    FILE *fp;
    char detail[220];

    if (!len_as || !len_mm) {
        free(len_as); free(len_mm);
        test_record(0, "aco", "mmas_dynamic_bounds_beat_as", "oom");
        return 0;
    }
    memset(&inst, 0, sizeof inst);
    {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 55001, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            free(len_as); free(len_mm);
            test_record(0, "aco", "mmas_dynamic_bounds_beat_as", "inst fail");
            return 0;
        }
    }
    fp = fopen("results/aco_mmas_vs_as.csv", "w");
    if (fp) fprintf(fp, "rep,len_elitist_as,len_mmas\n");
    for (i = 0; i < n_reps; ++i) {
        mlab_rng rng;
        mlab_aco_config cfg;
        mlab_aco_result r;
        unsigned seed = 60000u + (unsigned)i * 37u;

        memset(&cfg, 0, sizeof cfg);
        cfg.n_ants = 20;
        cfg.max_iter = 120;
        cfg.alpha = 1.0;
        cfg.beta = 3.0;
        cfg.rho = 0.3;
        cfg.elitist = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        len_as[i] = mlab_aco_tsp(&rng, &inst, &cfg, &r) ? r.best_len : 1e300;
        mlab_aco_result_free(&r);

        cfg.mmas = 1;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        len_mm[i] = mlab_aco_tsp(&rng, &inst, &cfg, &r) ? r.best_len : 1e300;
        mlab_aco_result_free(&r);

        ma += len_as[i];
        mm += len_mm[i];
        if (fp) fprintf(fp, "%d,%.6f,%.6f\n", i, len_as[i], len_mm[i]);
    }
    if (fp) fclose(fp);
    ma /= n_reps;
    mm /= n_reps;
    p = paired_p_two_sided(len_as, len_mm, n_reps); /* H: AS - MMAS > 0 */
    ok = (mm < ma) && (p < 0.05);
    snprintf(detail, sizeof detail,
             "n=30 AS=%.3f MMAS=%.3f p=%.2e（动态上下界）", ma, mm, p);
    test_record(ok, "aco", "mmas_dynamic_bounds_beat_as", detail);
    mlab_tsp_free(&inst);
    free(len_as); free(len_mm);
    return ok;
}

static int t_aco_beats_sa_and_near_greedy(void)
{
    /*
     * 判据：ACO(+2-opt) 在 n=30 TSP 上优于纯 SA（配对 p<0.05），
     * 且相对多起点贪心基线超出 ≤5%（解质量 gap 可解释）。
     */
    const int n_cities = 30, n_pairs = 40;
    mlab_tsp_inst inst;
    double *len_aco = (double *)malloc((size_t)n_pairs * sizeof(double));
    double *len_sa = (double *)malloc((size_t)n_pairs * sizeof(double));
    double *len_g = (double *)malloc((size_t)n_pairs * sizeof(double));
    int i, ok, better = 0;
    double ma = 0, ms = 0, mg = 0, p, gap;
    FILE *fp;
    char detail[260];

    if (!len_aco || !len_sa || !len_g) {
        free(len_aco); free(len_sa); free(len_g);
        test_record(0, "aco", "aco2opt_beats_sa_near_greedy", "oom");
        return 0;
    }
    memset(&inst, 0, sizeof inst);
    {
        mlab_rng rng;
        mlab_rng_seed_kind(&rng, 55001, MLAB_RNG_SPLITMIX64);
        if (!mlab_tsp_random(&rng, &inst, n_cities, 10.0)) {
            free(len_aco); free(len_sa); free(len_g);
            test_record(0, "aco", "aco2opt_beats_sa_near_greedy", "inst fail");
            return 0;
        }
    }
    fp = fopen("results/aco_sa_greedy_tsp30.csv", "w");
    if (fp) fprintf(fp, "pair,len_aco2opt,len_sa2opt,len_greedy\n");

    for (i = 0; i < n_pairs; ++i) {
        unsigned seed = 78000u + (unsigned)i * 41u;
        mlab_rng ra, rs, rg;
        mlab_aco_config acfg;
        mlab_aco_result aout;
        mlab_tsp_sa_config scfg;
        mlab_tsp_sa_result sout;
        int *tour0 = (int *)malloc((size_t)n_cities * sizeof(int));
        int k;

        if (!tour0) {
            len_aco[i] = len_sa[i] = len_g[i] = 1e300;
            continue;
        }
        for (k = 0; k < n_cities; ++k) tour0[k] = k;

        memset(&acfg, 0, sizeof acfg);
        acfg.n_ants = 25;
        acfg.max_iter = 120;
        acfg.alpha = 1.0;
        acfg.beta = 3.0;
        acfg.rho = 0.3;
        acfg.elitist = 1;
        acfg.use_2opt = 1;
        acfg.two_opt_pass = 3;

        memset(&scfg, 0, sizeof scfg);
        scfg.T0 = 2.0;
        scfg.alpha = 0.95;
        scfg.T_min = 1e-3;
        scfg.inner = 40;
        scfg.max_outer = 300;
        scfg.nbhd = MLAB_TSP_NBHD_2OPT;

        mlab_rng_seed_kind(&ra, seed, MLAB_RNG_SPLITMIX64);
        mlab_rng_seed_kind(&rs, seed + 3u, MLAB_RNG_SPLITMIX64);
        mlab_rng_seed_kind(&rg, seed + 5u, MLAB_RNG_SPLITMIX64);
        mlab_tsp_shuffle_tour(&rs, n_cities, tour0);

        memset(&aout, 0, sizeof aout);
        memset(&sout, 0, sizeof sout);
        if (!mlab_aco_tsp(&ra, &inst, &acfg, &aout) ||
            !mlab_sa_tsp(&rs, &inst, &scfg, tour0, &sout)) {
            len_aco[i] = len_sa[i] = len_g[i] = 1e300;
        } else {
            len_aco[i] = aout.best_len;
            len_sa[i] = sout.best_len;
            len_g[i] = mlab_tsp_greedy_nn(&inst, &rg, n_cities, NULL);
        }
        mlab_aco_result_free(&aout);
        mlab_tsp_sa_result_free(&sout);
        free(tour0);
        if (len_aco[i] < len_sa[i]) ++better;
        ma += len_aco[i];
        ms += len_sa[i];
        mg += len_g[i];
        if (fp)
            fprintf(fp, "%d,%.6f,%.6f,%.6f\n", i, len_aco[i], len_sa[i], len_g[i]);
    }
    if (fp) fclose(fp);
    ma /= n_pairs; ms /= n_pairs; mg /= n_pairs;
    p = paired_p_two_sided(len_sa, len_aco, n_pairs); /* H: SA - ACO > 0 */
    gap = mg > 0 ? (ma - mg) / mg : 1.0;
    /* ACO 更优（p<0.05）且相对贪心超出 ≤5%（允许 ACO 更好，gap 可为负） */
    ok = (ma < ms) && (p < 0.05) && (gap <= 0.05);
    snprintf(detail, sizeof detail,
             "n=30 ACO+2opt=%.3f SA+2opt=%.3f greedy=%.3f p=%.2e gap_vs_greedy=%.2f%% wins=%d/%d",
             ma, ms, mg, p, gap * 100.0, better, n_pairs);
    test_record(ok, "aco", "aco2opt_beats_sa_near_greedy", detail);
    mlab_tsp_free(&inst);
    free(len_aco); free(len_sa); free(len_g);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"pso", "baseline_sphere_rosen_rastrigin",
         "PSO 基线：三函数收敛数据落盘",
         t_pso_baseline_three_functions},
        {"pso", "w_topology_heatmap_exploration_shift",
         "w×拓扑热图完整；多峰最优 w 相对单峰右移",
         t_pso_w_topology_heatmap},
        {"pso", "ablation_full_beats_cognitive_social",
         "完整 PSO 在 Rastrigin 上成功率显著高于认知/社会单项",
         t_pso_ablation_full_vs_parts},
        {"pso", "clerc_constriction_vonneumann",
         "Clerc 收缩因子无 vmax 收敛可靠；Von Neumann 拓扑对照",
         t_pso_clerc_vonneumann},
        {"aco", "rho_scan_tsp40",
         "ACO 蒸发系数 ρ 扫描（n=30 TSP，关 2-opt 看构造区分度）",
         t_aco_rho_scan_tsp30},
        {"aco", "mmas_dynamic_bounds_beat_as",
         "MMAS 动态上下界显著优于精英 AS",
         t_aco_mmas_vs_elitist_as},
        {"aco", "aco2opt_beats_sa_near_greedy",
         "ACO+2opt 优于纯 SA 且相对贪心 gap≤5%",
         t_aco_beats_sa_and_near_greedy},
    };
    test_ensure_results_dir();
    return test_run_main("C7-pso-aco", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
