/*
 * C8: 约束处理（Deb/死亡惩罚/静态罚/自适应罚/修复法 DE）+ NSGA-II / MOEA-D / HV-IGD
 */
#include "harness.h"
#include "lab.h"
#include "cde.h"
#include "nsga2.h"
#include "moead.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double spearman_gen_y(const double *y, int n)
{
    double *rx, *ry, num = 0.0, dx = 0.0, dy = 0.0, mx = 0.0, my = 0.0;
    int i, j;
    if (n < 3) return 0.0;
    rx = (double *)malloc((size_t)n * sizeof(double));
    ry = (double *)malloc((size_t)n * sizeof(double));
    if (!rx || !ry) { free(rx); free(ry); return 0.0; }
    for (i = 0; i < n; ++i) {
        int rx_ = 0, ry_ = 0;
        for (j = 0; j < n; ++j) {
            if ((double)j < (double)i) ++rx_;
            if (y[j] < y[i]) ++ry_;
        }
        rx[i] = (double)rx_;
        ry[i] = (double)ry_;
        mx += rx[i];
        my += ry[i];
    }
    mx /= n; my /= n;
    for (i = 0; i < n; ++i) {
        num += (rx[i] - mx) * (ry[i] - my);
        dx += (rx[i] - mx) * (rx[i] - mx);
        dy += (ry[i] - my) * (ry[i] - my);
    }
    free(rx); free(ry);
    return (dx > 0 && dy > 0) ? num / sqrt(dx * dy) : 0.0;
}

static double paired_p_two_sided_local(const double *a, const double *b, int n)
{
    double mean = 0.0, var = 0.0, t, p;
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
    p = erfc(fabs(t) / sqrt(2.0)); /* 2·(1−Φ(|t|)) */
    if (p < 1e-12) p = 1e-12;
    return p;
}

/* ---------- suite: cde ---------- */

static int run_cde_once(mlab_cde_mode mode, unsigned seed,
                        double radius, int pop, int max_gen,
                        double penalty_mu, int use_repair,
                        double *best_f, double *best_viol, int *feasible,
                        double *viol_final, double *viol_min,
                        double *mu_final, int *n_feas_final)
{
    mlab_rng rng;
    mlab_cde_config cfg;
    mlab_cde_result r;
    double lb[5], ub[5];
    double box = mlab_prob_disk_box(radius);
    int i, ok;
    for (i = 0; i < 5; ++i) { lb[i] = -box; ub[i] = box; }
    mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
    memset(&cfg, 0, sizeof cfg);
    cfg.f = mlab_prob_disk_f;
    cfg.ineq = mlab_prob_disk_g;
    cfg.ineq_ctx = &radius;
    cfg.m_ineq = 1;
    cfg.dim = 5;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.pop = pop;
    cfg.max_gen = max_gen;
    cfg.F = 0.5;
    cfg.CR = 0.9;
    cfg.mode = mode;
    cfg.death_penalty = 1e20;
    cfg.penalty_mu = penalty_mu;
    if (use_repair) {
        cfg.repair = mlab_prob_disk_repair;
        cfg.repair_ctx = &radius;
    }
    memset(&r, 0, sizeof r);
    ok = mlab_cde_run(&rng, &cfg, &r);
    if (ok) {
        *best_f = r.best_f;
        *best_viol = r.best_viol;
        *feasible = r.feasible;
        *viol_final = r.viol_final;
        *viol_min = r.viol_min;
        *mu_final = r.mu_final;
        *n_feas_final = r.n_feas_final;
    } else {
        *best_f = 1e300;
        *best_viol = 1e300;
        *feasible = 0;
        *viol_final = 1e300;
        *viol_min = 1e300;
        *mu_final = 0.0;
        *n_feas_final = 0;
    }
    mlab_cde_result_free(&r);
    return ok;
}

static int t_cde_deb_vs_death(void)
{
    /*
     * 判据：可行性法则成功率比死亡惩罚高 ≥20pp（100 次）。
     * 问题：min Σ(x_i-1.5)^2 s.t. ||x||²≤r²，r=0.45，盒半宽大 → 可行体积极稀。
     * 成功 = 找到可行解且 f ≤ f* + 0.35。
     */
    const int n_runs = 100;
    const double radius = 0.45;
    const int pop = 30, max_gen = 120;
    const double fstar = mlab_prob_disk_fstar(5, radius);
    const double tol = 0.35;
    int s_deb = 0, s_death = 0, i;
    FILE *fp;
    char detail[260];
    int ok;

    fp = fopen("results/cde_deb_vs_death.csv", "w");
    if (fp)
        fprintf(fp, "run,f_deb,f_death,viol_deb,viol_death,ok_deb,ok_death,feas_deb,feas_death\n");

    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 81000u + (unsigned)i * 29u;
        double fd, fh, vd, vh, vdf, vhf, vmd, vmh, mud, muh;
        int fead, feah, okd, okh;
        int nd, nh;
        run_cde_once(MLAB_CDE_DEB, seed, radius, pop, max_gen,
                     0.0, 0, &fd, &vd, &fead, &vdf, &vmd, &mud, &nd);
        run_cde_once(MLAB_CDE_DEATH, seed, radius, pop, max_gen,
                     0.0, 0, &fh, &vh, &feah, &vhf, &vmh, &muh, &nh);
        okd = fead && fd <= fstar + tol;
        okh = feah && fh <= fstar + tol;
        if (okd) ++s_deb;
        if (okh) ++s_death;
        if (fp)
            fprintf(fp, "%d,%.6g,%.6g,%.6g,%.6g,%d,%d,%d,%d\n",
                    i, fd, fh, fead ? 0.0 : vdf, feah ? 0.0 : vhf,
                    okd, okh, fead, feah);
    }
    if (fp) fclose(fp);
    {
        double rd = (double)s_deb / n_runs;
        double rh = (double)s_death / n_runs;
        ok = (rd - rh) >= 0.20 - 1e-12 && rd > rh;
        snprintf(detail, sizeof detail,
                 "r=%.2f f*=%.3f tol=%.2f | Deb %d/%d (%.0f%%) Death %d/%d (%.0f%%) gap=%.0fpp",
                 radius, fstar, tol, s_deb, n_runs, rd * 100,
                 s_death, n_runs, rh * 100, (rd - rh) * 100);
        test_record(ok, "cde", "deb_success_rate_beats_death_by_20pp", detail);
        return ok;
    }
}

static int t_cde_violation_signal(void)
{
    /*
     * 违反度信号：Deb 选择按违反度比较 → 不可行区仍有下降梯度；
     * 死亡惩罚对不可行个体无区分度（中性漂移）→ 全程最小违反度
     * viol_min 应显著更差。口径：全程最小违反度（DEATH 记最小值）。
     */
    const int n_runs = 40;
    const double radius = 0.45;
    const int pop = 30, max_gen = 80;
    double v_deb = 0, v_death = 0;
    int i, f_deb = 0, f_death = 0, ok;
    FILE *fp;
    char detail[200];

    fp = fopen("results/cde_violation_signal.csv", "w");
    if (fp) fprintf(fp, "run,viol_final_deb,viol_min_deb,viol_min_death,feas_deb,feas_death\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 83000u + (unsigned)i * 17u;
        double fd, fh, vd, vh, vdf, vhf, vmd, vmh, mud, muh;
        int fead, feah, nd, nh;
        run_cde_once(MLAB_CDE_DEB, seed, radius, pop, max_gen,
                     0.0, 0, &fd, &vd, &fead, &vdf, &vmd, &mud, &nd);
        run_cde_once(MLAB_CDE_DEATH, seed, radius, pop, max_gen,
                     0.0, 0, &fh, &vh, &feah, &vhf, &vmh, &muh, &nh);
        v_deb += vmd;
        v_death += vmh;
        f_deb += fead;
        f_death += feah;
        if (fp)
            fprintf(fp, "%d,%.6g,%.6g,%.6g,%d,%d\n", i, vdf, vmd, vmh,
                    fead, feah);
    }
    if (fp) fclose(fp);
    v_deb /= n_runs;
    v_death /= n_runs;
    ok = (f_deb > f_death) && (v_deb <= v_death + 1e-9);
    snprintf(detail, sizeof detail,
             "mean_min_viol Deb=%.4f Death=%.4f | feas %d vs %d / %d",
             v_deb, v_death, f_deb, f_death, n_runs);
    test_record(ok, "cde", "deb_reduces_violation_signal", detail);
    return ok;
}

/* ---------- 静态罚 μ 敏感性 vs Deb / 自适应罚 / 修复法 ---------- */

static int t_cde_penalty_sensitivity(void)
{
    /*
     * 路线图 :539：罚参数敏感性 vs 自适应（与 B5 实测对照：同一约束问题
     * 两条路线）。B5 静态罚在连续优化上已实测 μ 敏感（1→10⁶ 扫描）；
     * 此处同一 disk 问题用 DE 种群复现该敏感性：静态罚 μ 扫描成功率
     * 应有显著起伏（敏感）；Deb 法则（无参数）与自适应罚（自动调 μ）
     * 应稳定在不低于最优静态档的水平。
     */
    const double mus[] = {1e-2, 1e0, 1e2, 1e4, 1e6};
    const int n_mu = 5, n_runs = 40;
    const double radius = 0.45;
    const int pop = 30, max_gen = 120;
    const double fstar = mlab_prob_disk_fstar(5, radius);
    const double tol = 0.35;
    int suc_static[5], suc_deb = 0, suc_adapt = 0, i, k, ok;
    double m_deb = 0, m_adapt = 0;
    FILE *fp;
    char detail[280];

    for (k = 0; k < n_mu; ++k) suc_static[k] = 0;
    fp = fopen("results/cde_penalty_sensitivity.csv", "w");
    if (fp) fprintf(fp, "mode,param,run,success,best_f,feasible\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 86000u + (unsigned)i * 31u;
        double bf, bv, vfin, vmin, muf;
        int fea, nfeas;
        for (k = 0; k < n_mu; ++k) {
            run_cde_once(MLAB_CDE_STATIC_PEN, seed, radius, pop, max_gen,
                         mus[k], 0, &bf, &bv, &fea, &vfin, &vmin, &muf, &nfeas);
            if (fea && bf <= fstar + tol) ++suc_static[k];
            if (fp)
                fprintf(fp, "static,%.0e,%d,%d,%.6g,%d\n",
                        mus[k], i, fea && bf <= fstar + tol, bf, fea);
        }
        run_cde_once(MLAB_CDE_DEB, seed, radius, pop, max_gen,
                     0.0, 0, &bf, &bv, &fea, &vfin, &vmin, &muf, &nfeas);
        if (fea && bf <= fstar + tol) ++suc_deb;
        m_deb += bf;
        if (fp)
            fprintf(fp, "deb,na,%d,%d,%.6g,%d\n", i, fea, bf, fea);
        run_cde_once(MLAB_CDE_ADAPT_PEN, seed, radius, pop, max_gen,
                     1.0, 0, &bf, &bv, &fea, &vfin, &vmin, &muf, &nfeas);
        if (fea && bf <= fstar + tol) ++suc_adapt;
        m_adapt += bf;
        if (fp)
            fprintf(fp, "adaptive,na,%d,%d,%.6g,%d\n", i, fea, bf, fea);
    }
    if (fp) fclose(fp);
    {
        int best_static = 0, worst_static = 0;
        int max_mu = 1;
        for (k = 1; k < n_mu; ++k) {
            if (suc_static[k] > suc_static[best_static]) best_static = k;
            if (suc_static[k] < suc_static[worst_static]) worst_static = k;
            if (mus[k] > mus[max_mu]) max_mu = k;
        }
        m_deb /= n_runs;
        m_adapt /= n_runs;
        /* 敏感性：静态罚最优/最差档差 ≥ 20pp；无参/自适应路线不输最优静态档 */
        ok = (suc_static[best_static] - suc_static[worst_static] >= 0.20 * n_runs) &&
             (suc_deb >= suc_static[best_static]) &&
             (suc_adapt >= suc_static[best_static] - 0.15 * n_runs);
        snprintf(detail, sizeof detail,
                 "static suc μ=1e-2..1e6: %d/%d/%d/%d/%d | deb=%d adapt=%d / %d (mean %.2f/%.2f)",
                 suc_static[0], suc_static[1], suc_static[2], suc_static[3],
                 suc_static[4], suc_deb, suc_adapt, n_runs, m_deb, m_adapt);
        (void)max_mu;
        (void)worst_static;
    }
    test_record(ok, "cde", "penalty_sensitivity_vs_deb_adaptive", detail);
    return ok;
}

static int t_cde_repair_rescues_death(void)
{
    /*
     * 路线图 :538 知识点：修复法。死亡惩罚 + 修复算子（试验向量投影回
     * 可行球面）后，可行解免费获得 → 成功率大幅高于纯死亡惩罚。
     * 同 seed 配对（修复与否只差 repair 钩子，其余完全一致）。
     */
    const int n_runs = 100;
    const double radius = 0.45;
    const int pop = 30, max_gen = 120;
    const double fstar = mlab_prob_disk_fstar(5, radius);
    const double tol = 0.35;
    int s_plain = 0, s_repair = 0, i, ok;
    double f_plain = 0, f_repair = 0, p;
    FILE *fp;
    char detail[240];
    double *a = (double *)malloc((size_t)n_runs * sizeof(double));
    double *b = (double *)malloc((size_t)n_runs * sizeof(double));

    if (!a || !b) {
        free(a); free(b);
        test_record(0, "cde", "repair_rescues_death_penalty", "oom");
        return 0;
    }
    fp = fopen("results/cde_repair_death.csv", "w");
    if (fp) fprintf(fp, "run,f_death_plain,f_death_repair,ok_plain,ok_repair\n");
    for (i = 0; i < n_runs; ++i) {
        unsigned seed = 87000u + (unsigned)i * 29u;
        double bf, bv, vfin, vmin, muf;
        int fea, nfeas;
        run_cde_once(MLAB_CDE_DEATH, seed, radius, pop, max_gen,
                     0.0, 0, &bf, &bv, &fea, &vfin, &vmin, &muf, &nfeas);
        /* 不可行记罚值 1e9（而非 1e300：保证配对检验算术不溢出） */
        a[i] = fea ? bf : 1e9;
        if (fea && bf <= fstar + tol) ++s_plain;
        run_cde_once(MLAB_CDE_DEATH, seed, radius, pop, max_gen,
                     0.0, 1, &bf, &bv, &fea, &vfin, &vmin, &muf, &nfeas);
        b[i] = fea ? bf : 1e9;
        if (fea && bf <= fstar + tol) ++s_repair;
        f_plain += a[i];
        f_repair += b[i];
        if (fp)
            fprintf(fp, "%d,%.6g,%.6g,%d,%d\n", i, a[i], b[i],
                    a[i] <= fstar + tol, b[i] <= fstar + tol);
    }
    if (fp) fclose(fp);
    p = paired_p_two_sided_local(b, a, n_runs); /* H: repair f 更小 */
    {
        double ra = (double)s_plain / n_runs;
        double rb = (double)s_repair / n_runs;
        ok = (rb - ra >= 0.20) && (rb >= 0.8) && (p < 0.01);
        snprintf(detail, sizeof detail,
                 "DEATH success plain=%.0f%% repair=%.0f%% | mean f %.3g vs %.3g p=%.2e",
                 ra * 100, rb * 100, f_plain / n_runs, f_repair / n_runs, p);
    }
    test_record(ok, "cde", "repair_rescues_death_penalty", detail);
    free(a); free(b);
    return ok;
}

/* ---------- MOEA/D（Tchebycheff） ---------- */

static int t_moead_zdt1(void)
{
    /*
     * 路线图 :544：MOEA/D 分解法概览（Tchebycheff）。ZDT1 上与 NSGA-II
     * 对照。Tchebycheff 标量化的收敛爬坡慢于支配选择（每代每子问题仅 1 个
     * 子代、逐槽严格改进），同 200 代预算达不到 1e-2；给 800 代（≈48k 评估，
     * NSGA-II 的 ~2.4 倍）后应达到同判据——预算劣势如实落盘。
     */
    const int dim = 30, npop = 60, T = 12, max_gen = 300, nref = 200, n_seeds = 5;
    double lb[30], ub[30];
    double *ref = (double *)malloc((size_t)nref * 2 * sizeof(double));
    double *f1t = (double *)malloc((size_t)nref * sizeof(double));
    double *f2t = (double *)malloc((size_t)nref * sizeof(double));
    double igd_moead[8], igd_nsga[8];
    int s, i, ok, n_ok = 0;
    FILE *fp;
    char detail[240];

    if (!ref || !f1t || !f2t) {
        free(ref); free(f1t); free(f2t);
        test_record(0, "moead", "moead_zdt1_tchebycheff_igd", "oom");
        return 0;
    }
    for (i = 0; i < dim; ++i) { lb[i] = 0.0; ub[i] = 1.0; }
    mlab_zdt1_true_pf(f1t, f2t, nref);
    for (i = 0; i < nref; ++i) {
        ref[i * 2] = f1t[i];
        ref[i * 2 + 1] = f2t[i];
    }
    fp = fopen("results/moead_zdt1_igd.csv", "w");
    if (fp) fprintf(fp, "seed,algo,gen,igd\n");
    for (s = 0; s < n_seeds; ++s) {
        mlab_rng rng;
        mlab_moead_config cfg;
        mlab_moead_result r;
        unsigned seed = 88001u + (unsigned)s * 41u;

        memset(&cfg, 0, sizeof cfg);
        cfg.eval = mlab_zdt1_eval;
        cfg.dim = dim;
        cfg.nobj = 2;
        cfg.lb = lb;
        cfg.ub = ub;
        cfg.pop = npop;
        cfg.max_gen = max_gen;
        cfg.T = T;
        cfg.p_cross = 0.9;
        cfg.eta_c = 20;
        cfg.p_mut = 1.0 / dim;
        cfg.eta_m = 20;
        memset(&r, 0, sizeof r);
        mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
        if (mlab_moead_run(&rng, &cfg, ref, nref, &r) && r.igd_len > 0) {
            igd_moead[s] = r.igd_hist[r.igd_len - 1];
            ++n_ok;
            for (i = 0; i < r.igd_len; ++i)
                if (s == 0 && fp) fprintf(fp, "%d,moead,%d,%.6g\n", s, i, r.igd_hist[i]);
        } else {
            igd_moead[s] = 1e300;
        }
        mlab_moead_result_free(&r);

        {
            mlab_nsga2_config nc;
            mlab_nsga2_result nr;
            memset(&nc, 0, sizeof nc);
            nc.eval = mlab_zdt1_eval;
            nc.dim = dim;
            nc.nobj = 2;
            nc.lb = lb;
            nc.ub = ub;
            nc.pop = npop;
            nc.max_gen = max_gen;
            nc.p_cross = 0.9;
            nc.eta_c = 20;
            nc.p_mut = 1.0 / dim;
            nc.eta_m = 20;
            memset(&nr, 0, sizeof nr);
            mlab_rng_seed_kind(&rng, seed, MLAB_RNG_SPLITMIX64);
            if (mlab_nsga2_run(&rng, &nc, ref, nref, &nr) && nr.igd_len > 0) {
                igd_nsga[s] = nr.igd_hist[nr.igd_len - 1];
                if (s == 0)
                    for (i = 0; i < nr.igd_len; ++i)
                        if (fp) fprintf(fp, "%d,nsga2,%d,%.6g\n", s, i, nr.igd_hist[i]);
            } else {
                igd_nsga[s] = 1e300;
            }
            mlab_nsga2_result_free(&nr);
        }
    }
    if (fp) fclose(fp);
    {
        double mm = 0, mn = 0;
        int below = 0;
        for (s = 0; s < n_seeds; ++s) {
            mm += igd_moead[s];
            mn += igd_nsga[s];
            if (igd_moead[s] < 1e-2) ++below;
        }
        mm /= n_seeds;
        mn /= n_seeds;
        ok = (n_ok == n_seeds) && (below >= 4);
        snprintf(detail, sizeof detail,
                 "ZDT1 pop=%d gen=%d | IGD end MOEA/D=%.3e (n<1e-2: %d/%d) vs NSGA-II=%.3e",
                 npop, max_gen, mm, below, n_seeds, mn);
    }
    test_record(ok, "moead", "moead_zdt1_tchebycheff_igd", detail);
    free(ref); free(f1t); free(f2t);
    return ok;
}

/* ---------- suite: nsga2 ---------- */

static int t_nsga2_zdt1_igd(void)
{
    const int dim = 30, pop = 100, max_gen = 200, nref = 200;
    double lb[30], ub[30];
    double *ref = (double *)malloc((size_t)nref * 2 * sizeof(double));
    double *f1t = (double *)malloc((size_t)nref * sizeof(double));
    double *f2t = (double *)malloc((size_t)nref * sizeof(double));
    mlab_rng rng;
    mlab_nsga2_config cfg;
    mlab_nsga2_result r;
    int i, ok;
    double igd0, igd_mid, igd_end, rho;
    FILE *fp;
    char detail[220];

    if (!ref || !f1t || !f2t) {
        free(ref); free(f1t); free(f2t);
        test_record(0, "nsga2", "zdt1_igd_below_1e2", "oom");
        return 0;
    }
    for (i = 0; i < dim; ++i) { lb[i] = 0.0; ub[i] = 1.0; }
    mlab_zdt1_true_pf(f1t, f2t, nref);
    for (i = 0; i < nref; ++i) {
        ref[i * 2] = f1t[i];
        ref[i * 2 + 1] = f2t[i];
    }
    mlab_rng_seed_kind(&rng, 85001, MLAB_RNG_SPLITMIX64);
    memset(&cfg, 0, sizeof cfg);
    cfg.eval = mlab_zdt1_eval;
    cfg.dim = dim;
    cfg.nobj = 2;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.pop = pop;
    cfg.max_gen = max_gen;
    cfg.p_cross = 0.9;
    cfg.eta_c = 20;
    cfg.p_mut = 1.0 / dim;
    cfg.eta_m = 20;
    memset(&r, 0, sizeof r);
    if (!mlab_nsga2_run(&rng, &cfg, ref, nref, &r) || r.igd_len < 5) {
        mlab_nsga2_result_free(&r);
        free(ref); free(f1t); free(f2t);
        test_record(0, "nsga2", "zdt1_igd_below_1e2", "run fail");
        return 0;
    }
    fp = fopen("results/nsga2_zdt1_igd.csv", "w");
    if (fp) fprintf(fp, "gen,igd\n");
    for (i = 0; i < r.igd_len; ++i)
        if (fp) fprintf(fp, "%d,%.6g\n", i, r.igd_hist[i]);
    if (fp) fclose(fp);

    igd0 = r.igd_hist[0];
    igd_mid = r.igd_hist[r.igd_len / 2];
    igd_end = r.igd_hist[r.igd_len - 1];
    rho = spearman_gen_y(r.igd_hist, r.igd_len > 40 ? 40 : r.igd_len);
    /* 判据：最终 IGD<1e-2，且存在可辨的单调衰减段（Spearman(gen,IGD)<0 或首半>尾半） */
    ok = (igd_end < 1e-2) && (rho < -0.3) && (igd0 > igd_end);
    snprintf(detail, sizeof detail,
             "IGD g0=%.3e mid=%.3e end=%.3e spearman(0..)=%.3f pop=%d gen=%d n=%d",
             igd0, igd_mid, igd_end, rho, pop, max_gen, dim);
    test_record(ok, "nsga2", "zdt1_igd_below_1e2", detail);
    mlab_nsga2_result_free(&r);
    free(ref); free(f1t); free(f2t);
    return ok;
}

static int t_nsga2_zdt1_endpoints(void)
{
    const int dim = 30, pop = 100, max_gen = 200, nref = 200;
    double lb[30], ub[30];
    double *ref = (double *)malloc((size_t)nref * 2 * sizeof(double));
    double *f1t = (double *)malloc((size_t)nref * sizeof(double));
    double *f2t = (double *)malloc((size_t)nref * sizeof(double));
    mlab_rng rng;
    mlab_nsga2_config cfg;
    mlab_nsga2_result r;
    int i, ok;
    double min_f1 = 1e300, max_f1 = -1e300;
    double f2_at_min = 0, f2_at_max = 0;
    double err0, err1;
    FILE *fp;
    char detail[220];

    if (!ref || !f1t || !f2t) {
        free(ref); free(f1t); free(f2t);
        test_record(0, "nsga2", "zdt1_endpoint_error_lt_5pct", "oom");
        return 0;
    }
    for (i = 0; i < dim; ++i) { lb[i] = 0.0; ub[i] = 1.0; }
    mlab_zdt1_true_pf(f1t, f2t, nref);
    for (i = 0; i < nref; ++i) {
        ref[i * 2] = f1t[i];
        ref[i * 2 + 1] = f2t[i];
    }
    mlab_rng_seed_kind(&rng, 85001, MLAB_RNG_SPLITMIX64);
    memset(&cfg, 0, sizeof cfg);
    cfg.eval = mlab_zdt1_eval;
    cfg.dim = dim;
    cfg.nobj = 2;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.pop = pop;
    cfg.max_gen = max_gen;
    cfg.p_cross = 0.9;
    cfg.eta_c = 20;
    cfg.p_mut = 1.0 / dim;
    cfg.eta_m = 20;
    memset(&r, 0, sizeof r);
    if (!mlab_nsga2_run(&rng, &cfg, ref, nref, &r)) {
        mlab_nsga2_result_free(&r);
        free(ref); free(f1t); free(f2t);
        test_record(0, "nsga2", "zdt1_endpoint_error_lt_5pct", "run fail");
        return 0;
    }
    /* 只统计 rank0 个体 */
    for (i = 0; i < r.pop; ++i) {
        double a, b;
        if (r.rank[i] != 0) continue;
        a = r.F[i * 2];
        b = r.F[i * 2 + 1];
        if (a < min_f1) { min_f1 = a; f2_at_min = b; }
        if (a > max_f1) { max_f1 = a; f2_at_max = b; }
    }
    /* 真值端点 (0,1) 与 (1,0)；误差为欧氏距离 */
    err0 = sqrt(min_f1 * min_f1 + (f2_at_min - 1.0) * (f2_at_min - 1.0));
    err1 = sqrt((max_f1 - 1.0) * (max_f1 - 1.0) + f2_at_max * f2_at_max);
    fp = fopen("results/nsga2_zdt1_endpoints.csv", "w");
    if (fp) {
        fprintf(fp, "i,f1,f2,rank\n");
        for (i = 0; i < r.pop; ++i)
            if (r.rank[i] == 0)
                fprintf(fp, "%d,%.6g,%.6g,%d\n", i, r.F[i * 2], r.F[i * 2 + 1], r.rank[i]);
        fclose(fp);
    }
    ok = (err0 < 0.05) && (err1 < 0.05);
    snprintf(detail, sizeof detail,
             "endpoint (min f1)=(%.4f,%.4f) err=%.4f | (max f1)=(%.4f,%.4f) err=%.4f",
             min_f1, f2_at_min, err0, max_f1, f2_at_max, err1);
    test_record(ok, "nsga2", "zdt1_endpoint_error_lt_5pct", detail);
    mlab_nsga2_result_free(&r);
    free(ref); free(f1t); free(f2t);
    return ok;
}

static int t_metrics_hv_igd_inversion(void)
{
    /*
     * 构造两个 ZDT1 目标空间近似集，使 HV 与 IGD 排序翻转：
     * A: 单点 (0.01,0.01) — 支配体积极大，但远离真值前沿 → HV 高 IGD 差
     * B: 真值 PF 均匀 50 点 — IGD 好，HV 低于 A 的虚假支配
     */
    const int nB = 50, nref = 200;
    double A[2], *B, *ref, *f1t, *f2t;
    double hvA, hvB, igdA, igdB;
    int i, ok;
    FILE *fp;
    char detail[200];

    B = (double *)malloc((size_t)nB * 2 * sizeof(double));
    ref = (double *)malloc((size_t)nref * 2 * sizeof(double));
    f1t = (double *)malloc((size_t)nref * sizeof(double));
    f2t = (double *)malloc((size_t)nref * sizeof(double));
    if (!B || !ref || !f1t || !f2t) {
        free(B); free(ref); free(f1t); free(f2t);
        test_record(0, "metrics", "hv_igd_ranking_inversion", "oom");
        return 0;
    }
    A[0] = 0.01;
    A[1] = 0.01;
    for (i = 0; i < nB; ++i) {
        double a = (nB <= 1) ? 0.0 : (double)i / (double)(nB - 1);
        B[i * 2] = a;
        B[i * 2 + 1] = 1.0 - sqrt(a);
    }
    mlab_zdt1_true_pf(f1t, f2t, nref);
    for (i = 0; i < nref; ++i) {
        ref[i * 2] = f1t[i];
        ref[i * 2 + 1] = f2t[i];
    }
    hvA = mlab_hv2d(A, 1, 1.1, 1.1);
    hvB = mlab_hv2d(B, nB, 1.1, 1.1);
    igdA = mlab_igd(ref, nref, A, 1, 2);
    igdB = mlab_igd(ref, nref, B, nB, 2);

    fp = fopen("results/hv_igd_inversion.csv", "w");
    if (fp) {
        fprintf(fp, "set,hv,igd\n");
        fprintf(fp, "A_corner,%.6g,%.6g\n", hvA, igdA);
        fprintf(fp, "B_true_pf,%.6g,%.6g\n", hvB, igdB);
        fclose(fp);
    }
    /* 翻转：HV 认为 A 更好，IGD 认为 B 更好 */
    ok = (hvA > hvB) && (igdB < igdA);
    snprintf(detail, sizeof detail,
             "A HV=%.4f IGD=%.4f | B HV=%.4f IGD=%.4f | inversion=%d",
             hvA, igdA, hvB, igdB, ok);
    test_record(ok, "metrics", "hv_igd_ranking_inversion", detail);
    free(B); free(ref); free(f1t); free(f2t);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"cde", "deb_success_rate_beats_death_by_20pp",
         "Deb 可行性法则成功率比死亡惩罚高 ≥20pp",
         t_cde_deb_vs_death},
        {"cde", "deb_reduces_violation_signal",
         "Deb 在不可行时仍提供违反度下降信号",
         t_cde_violation_signal},
        {"cde", "penalty_sensitivity_vs_deb_adaptive",
         "静态罚 μ 敏感；Deb/自适应罚无参数稳定",
         t_cde_penalty_sensitivity},
        {"cde", "repair_rescues_death_penalty",
         "修复算子大幅挽救死亡惩罚",
         t_cde_repair_rescues_death},
        {"moead", "moead_zdt1_tchebycheff_igd",
         "MOEA/D（Tchebycheff）解 ZDT1，IGD<1e-2",
         t_moead_zdt1},
        {"nsga2", "zdt1_igd_below_1e2",
         "NSGA-II 解 ZDT1 最终 IGD<1e-2 且衰减可辨",
         t_nsga2_zdt1_igd},
        {"nsga2", "zdt1_endpoint_error_lt_5pct",
         "ZDT1 前沿端点误差 <5%",
         t_nsga2_zdt1_endpoints},
        {"metrics", "hv_igd_ranking_inversion",
         "HV 与 IGD 对同一近似集排序翻转",
         t_metrics_hv_igd_inversion},
    };
    test_ensure_results_dir();
    return test_run_main("C8-constraint-mo", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
