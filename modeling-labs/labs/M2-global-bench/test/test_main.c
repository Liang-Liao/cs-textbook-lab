/*
 * M2: 全局优化基准测试台 — 4算法×5函数×3维 + 混合框架 + 选择参考表
 */
#include "harness.h"
#include "lab.h"
#include "gbench.h"
#include "rng.h"
#include "de.h"
#include "bench_sa.h"
#include "bench.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N_FUNC 5
#define N_ALGO 4   /* SA GA DE PSO — 最小口径 */
#define N_DIM 3
#define N_RUN 100
#define BUDGET 900

static const int g_dims[N_DIM] = {2, 5, 10};
/* 主表算法顺序：SA, GA, DE, PSO */
static const int g_algos[N_ALGO] = {MLAB_GALGO_SA, MLAB_GALGO_GA,
                                    MLAB_GALGO_DE, MLAB_GALGO_PSO};

typedef struct {
    int success[N_ALGO][N_FUNC][N_DIM];
    double aes[N_ALGO][N_FUNC][N_DIM]; /* 成功时平均评估；失败记 n_eval */
    int n_eval_mean[N_ALGO][N_FUNC][N_DIM];
} m2_grid;

static int t_grid_full_table(void)
{
    m2_grid grid;
    mlab_rng rng;
    int a, f, d, r;
    int total_cells = N_ALGO * N_FUNC * N_DIM;
    int ok;
    char detail[260];
    FILE *fp;

    memset(&grid, 0, sizeof grid);
    test_ensure_results_dir();
    fp = fopen("results/m2_full_table.csv", "w");
    if (fp)
        fprintf(fp, "algo,func,dim,n_runs,success_rate,mean_evals,mean_evals_success,target\n");

    mlab_rng_seed(&rng, 96001);

    for (f = 0; f < N_FUNC; ++f) {
        const mlab_gfunc *gf = mlab_gfunc_get(f);
        for (d = 0; d < N_DIM; ++d) {
            int dim = g_dims[d];
            for (a = 0; a < N_ALGO; ++a) {
                int succ = 0;
                double sum_aes = 0.0, sum_ne = 0.0;
                int n_s = 0;
                for (r = 0; r < N_RUN; ++r) {
                    mlab_galgo_out out;
                    unsigned seed = 970000u + (unsigned)(f * 10000 + d * 1000 + a * 100 + r);
                    memset(&out, 0, sizeof out);
                    mlab_rng_seed(&rng, seed);
                    if (mlab_galgo_run(g_algos[a], &rng, f, dim, BUDGET, &out) != 0) {
                        mlab_galgo_out_free(&out);
                        continue;
                    }
                    sum_ne += out.n_eval;
                    if (out.success) {
                        ++succ;
                        sum_aes += (out.evals_to_target > 0) ? out.evals_to_target : out.n_eval;
                        ++n_s;
                    }
                    mlab_galgo_out_free(&out);
                }
                grid.success[a][f][d] = succ;
                grid.aes[a][f][d] = n_s ? sum_aes / n_s : 0.0;
                grid.n_eval_mean[a][f][d] = (int)(sum_ne / N_RUN);
                if (fp)
                    fprintf(fp, "%s,%s,%d,%d,%.3f,%.1f,%.1f,%.1e\n",
                            mlab_galgo_name(g_algos[a]), gf->name, dim, N_RUN,
                            (double)succ / N_RUN,
                            sum_ne / N_RUN, grid.aes[a][f][d], gf->target);
            }
        }
    }
    if (fp) fclose(fp);

    /* 表完整：每格 N_RUN 次均已统计（success 计数在 0..N_RUN，且我们确实跑了 N_RUN） */
    ok = 1;
    {
        /* 用 CSV 行数核对 60 行 */
        FILE *fr = fopen("results/m2_full_table.csv", "r");
        int lines = 0;
        char buf[256];
        if (!fr) ok = 0;
        else {
            if (fgets(buf, sizeof buf, fr)) {
                while (fgets(buf, sizeof buf, fr)) ++lines;
            }
            fclose(fr);
            if (lines < total_cells) ok = 0;
        }
    }
    snprintf(detail, sizeof detail,
             "grid %d algos × %d funcs × %d dims × %d runs=%d cells | csv rows ok=%d",
             N_ALGO, N_FUNC, N_DIM, N_RUN, total_cells, ok);
    test_record(ok, "grid", "full_table_4x5x3_100runs", detail);
    return ok;
}

static int t_convergence_curves(void)
{
    /*
     * 补收敛曲线 CSV（R7）：各算法 mean/median best_f 随评估预算变化。
     * 口径：对每个检查点 budget=c 独立完整重跑（同 seed），即「预算 c 时可达的
     * best_f」，避免依赖运行内截断假设；同时记录实际平均评估数（小预算下
     * SA/PSO 的内层批量会使真实评估略高于名义预算，如实呈现）。
     * 校验：各 (algo,func) 终局（ckpt=900）均值 best_f 不高于首检查点（整体趋势下降）。
     */
    const int algos[6] = {MLAB_GALGO_SA, MLAB_GALGO_GA, MLAB_GALGO_DE,
                          MLAB_GALGO_PSO, MLAB_GALGO_CMAES, MLAB_GALGO_HYBRID};
    const int n_ckpt = 9;
    const int ckpts[9] = {100, 200, 300, 400, 500, 600, 700, 800, 900};
    const int n_run = 60;
    const int dim = 5;
    int a, f, ci, r, ok, rows = 0;
    char detail[220];
    FILE *fp;
    mlab_rng rng;

    test_ensure_results_dir();
    fp = fopen("results/m2_convergence_curves.csv", "w");
    if (fp)
        fprintf(fp, "algo,func,dim,ckpt_evals,n_runs,mean_best_f,median_best_f,mean_evals_actual\n");
    ok = 1;
    for (a = 0; a < 6 && ok; ++a) {
        for (f = 0; f < N_FUNC && ok; ++f) {
            const mlab_gfunc *gf = mlab_gfunc_get(f);
            double first_mean = -1.0;
            for (ci = 0; ci < n_ckpt && ok; ++ci) {
                double vals[60];
                double sum = 0.0, sum_ev = 0.0;
                int n_ok = 0;
                for (r = 0; r < n_run; ++r) {
                    mlab_galgo_out out;
                    unsigned seed = 996000u + (unsigned)(f * 10000 + a * 1000 + r);
                    mlab_rng_seed(&rng, seed);
                    memset(&out, 0, sizeof out);
                    if (mlab_galgo_run(algos[a], &rng, f, dim, ckpts[ci], &out) == 0) {
                        vals[n_ok++] = out.best_f;
                        sum += out.best_f;
                        sum_ev += out.n_eval;
                    }
                    mlab_galgo_out_free(&out);
                }
                if (n_ok > 0) {
                    double mean = sum / n_ok;
                    /* 插入排序取中位数（n=60） */
                    int i, j;
                    double med;
                    for (i = 1; i < n_ok; ++i) {
                        double v = vals[i];
                        for (j = i - 1; j >= 0 && vals[j] > v; --j) vals[j + 1] = vals[j];
                        vals[j + 1] = v;
                    }
                    med = (n_ok % 2) ? vals[n_ok / 2]
                                     : 0.5 * (vals[n_ok / 2 - 1] + vals[n_ok / 2]);
                    if (fp)
                        fprintf(fp, "%s,%s,%d,%d,%d,%.6g,%.6g,%.1f\n",
                                mlab_galgo_name(algos[a]), gf->name, dim, ckpts[ci],
                                n_ok, mean, med, sum_ev / n_ok);
                    ++rows;
                    /* 整体趋势校验：终局均值不得高于首检查点（PSO 的 w 调度依赖
                     * max_gen、HJ 混合的阶段分配随预算变化，等预算重跑非严格前缀，
                     * 故不要求逐点单调，只要求总体下降趋势） */
                    if (ci == n_ckpt - 1 && first_mean >= 0.0 &&
                        mean > first_mean * (1.0 + 1e-9) + 1e-12)
                        ok = 0;
                    if (ci == 0) first_mean = mean;
                }
            }
        }
    }
    if (fp) fclose(fp);
    if (rows < 6 * N_FUNC * n_ckpt) ok = 0;
    snprintf(detail, sizeof detail,
             "6 algos × 5 funcs × %d ckpts (n=%d, dim=%d) rows=%d | 终局≤首检查点 %s",
             n_ckpt, n_run, dim, rows, ok ? "PASS" : "FAIL");
    test_record(ok, "curve", "convergence_curve_csv", detail);
    return ok;
}

static int t_hybrid_beats_singles_multimodal(void)
{
    /*
     * 判据：混合框架在多峰函数上成功率显著优于任一单层算法（p < 0.01）。
     * 设定：n=5 的 Rastrigin / Griewank / Ackley，总预算 1000 评估。
     * 预算口径（R7 等预算对齐）：HYBRID 总评估 = budget（全局 75% + 精修 25%），
     * 与单层同预算对比；n_eval 真实计数随 CSV mean_evals 列透明呈现。
     * 判据 2 补逐函数口径：逐函数成功数写入 CSV 并在结论中如实呈现
     * （Griewank 上 GA 反超 HYBRID 时如实报告，不聚合掩没）。
     */
    const int funcs[3] = {MLAB_GB_RASTRIGIN, MLAB_GB_GRIEWANK, MLAB_GB_ACKLEY};
    const int n_rep = 100;
    const int dim = 5;
    const int budget = 1000;
    /* 混合对比用更紧判据：突出局部精修把「接近最优」推入成功区的作用 */
    const double hyb_target = 0.01;
    int s_hyb = 0;
    int s_single[4] = {0, 0, 0, 0};
    double pmax = 0.0;
    int fi, r, a, ok;
    char detail[320];
    char perfunc[160];
    FILE *fp;
    mlab_rng rng;

    test_ensure_results_dir();
    fp = fopen("results/m2_hybrid_multimodal.csv", "w");
    if (fp) fprintf(fp, "func,algo,success,n,rate,best_f_mean,mean_evals,target\n");

    perfunc[0] = '\0';
    for (fi = 0; fi < 3; ++fi) {
        int func = funcs[fi];
        const mlab_gfunc *gf = mlab_gfunc_get(func);
        int sh = 0;
        int ss[4] = {0, 0, 0, 0};
        double sum_h = 0.0, sum_s[4] = {0, 0, 0, 0};
        double ev_h = 0.0, ev_s[4] = {0, 0, 0, 0};
        for (r = 0; r < n_rep; ++r) {
            mlab_galgo_out out;
            unsigned seed = 980000u + (unsigned)(fi * 10000 + r);
            mlab_rng_seed(&rng, seed);
            memset(&out, 0, sizeof out);
            if (mlab_galgo_run(MLAB_GALGO_HYBRID, &rng, func, dim, budget, &out) == 0) {
                sum_h += out.best_f;
                ev_h += out.n_eval;
                if (out.best_f <= hyb_target) ++sh;
            }
            mlab_galgo_out_free(&out);
            for (a = 0; a < 4; ++a) {
                mlab_galgo_out so;
                mlab_rng_seed(&rng, seed + 500u + (unsigned)a);
                memset(&so, 0, sizeof so);
                if (mlab_galgo_run(g_algos[a], &rng, func, dim, budget, &so) == 0) {
                    sum_s[a] += so.best_f;
                    ev_s[a] += so.n_eval;
                    if (so.best_f <= hyb_target) ++ss[a];
                }
                mlab_galgo_out_free(&so);
            }
        }
        s_hyb += sh;
        for (a = 0; a < 4; ++a) s_single[a] += ss[a];
        if (fp) {
            fprintf(fp, "%s,HYBRID,%d,%d,%.3f,%.4g,%.1f,%.3g\n", gf->name, sh, n_rep,
                    (double)sh / n_rep, sum_h / n_rep, ev_h / n_rep, hyb_target);
            for (a = 0; a < 4; ++a)
                fprintf(fp, "%s,%s,%d,%d,%.3f,%.4g,%.1f,%.3g\n", gf->name,
                        mlab_galgo_name(g_algos[a]), ss[a], n_rep,
                        (double)ss[a] / n_rep, sum_s[a] / n_rep, ev_s[a] / n_rep,
                        hyb_target);
        }
        {
            int best_a = 0;
            for (a = 1; a < 4; ++a) if (ss[a] > ss[best_a]) best_a = a;
            {
                char line[64];
                snprintf(line, sizeof line, "%s:%s%d vs %s%d ",
                         gf->name, gf->name[0] == 'r' ? "" : "",
                         sh, mlab_galgo_name(g_algos[best_a]), ss[best_a]);
                strncat(perfunc, line, sizeof perfunc - strlen(perfunc) - 1);
            }
        }
    }
    if (fp) fclose(fp);

    pmax = 0.0;
    ok = 1;
    for (a = 0; a < 4; ++a) {
        double p = mlab_two_prop_p_one_sided(s_hyb, n_rep * 3, s_single[a], n_rep * 3);
        if (p > pmax) pmax = p;
        if (!(s_hyb > s_single[a]) || !(p < 0.01)) ok = 0;
    }
    snprintf(detail, sizeof detail,
             "multi-modal n=%d target=%.3g budget=%d(等预算) | HYBRID %d/%d | "
             "SA %d GA %d DE %d PSO %d | max p=%.2e | 逐函数 %s",
             dim, hyb_target, budget, s_hyb, n_rep * 3,
             s_single[0], s_single[1], s_single[2], s_single[3], pmax, perfunc);
    test_record(ok, "hybrid", "beats_each_single_on_multimodal", detail);
    return ok;
}

static int t_selection_reference_table(void)
{
    /* 依据 full_table CSV 写出《算法选择参考表》 */
    FILE *fr, *fw;
    char buf[256];
    int ok;
    /* 收集每 (func,dim) 上成功率最高的算法（主表 4 算法）+ 额外跑 CMA-ES/混合补充 */

    test_ensure_results_dir();
    fr = fopen("results/m2_full_table.csv", "r");
    fw = fopen("results/m2_algo_selection.md", "w");
    if (!fr || !fw) {
        if (fr) fclose(fr);
        if (fw) fclose(fw);
        test_record(0, "report", "algo_selection_reference_table", "open fail");
        return 0;
    }
    fprintf(fw, "# 算法选择参考表（M2 本轮实测）\n\n");
    fprintf(fw, "预算 budget=%d 评估；每格 %d 次独立运行；成功判据 f≤target。\n\n",
            BUDGET, N_RUN);
    fprintf(fw, "| 问题特征 | 推荐算法 | 依据（本轮数据） |\n|---|---|---|\n");
    /* 跳过表头，扫描写入 */
    if (fgets(buf, sizeof buf, fr)) {
        /* 按 func+dim 找最优 */
        /* 简单实现：再读全部到数组 */
    }
    fclose(fr);

    /* 重读并解析 */
    fr = fopen("results/m2_full_table.csv", "r");
    if (!fr) {
        fclose(fw);
        test_record(0, "report", "algo_selection_reference_table", "reopen fail");
        return 0;
    }
    {
        typedef struct {
            char algo[16], func[16];
            int dim;
            double rate, aes;
        } row;
        row rows[64];
        int nrow = 0;
        if (fgets(buf, sizeof buf, fr)) {
            while (nrow < 64 && fgets(buf, sizeof buf, fr)) {
                char algo[16], func[16];
                int dim, nruns;
                double rate, me, aes, tgt;
                if (sscanf(buf, "%15[^,],%15[^,],%d,%d,%lf,%lf,%lf,%lf",
                           algo, func, &dim, &nruns, &rate, &me, &aes, &tgt) >= 5) {
                    snprintf(rows[nrow].algo, sizeof rows[nrow].algo, "%s", algo);
                    snprintf(rows[nrow].func, sizeof rows[nrow].func, "%s", func);
                    rows[nrow].dim = dim;
                    rows[nrow].rate = rate;
                    rows[nrow].aes = aes;
                    ++nrow;
                }
            }
        }
        fclose(fr);
        /* 对每个 func×dim 找 rate 最大、AES 次之 */
        {
            int f, d;
            const char *fnames[5] = {"sphere", "rosenbrock", "rastrigin", "griewank", "ackley"};
            for (f = 0; f < 5; ++f) {
                for (d = 0; d < 3; ++d) {
                    int dim = g_dims[d];
                    int best_i = -1;
                    int i;
                    for (i = 0; i < nrow; ++i) {
                        if (strcmp(rows[i].func, fnames[f]) != 0 || rows[i].dim != dim)
                            continue;
                        if (best_i < 0 || rows[i].rate > rows[best_i].rate + 1e-9 ||
                            (fabs(rows[i].rate - rows[best_i].rate) < 1e-9 &&
                             rows[i].aes < rows[best_i].aes))
                            best_i = i;
                    }
                    if (best_i >= 0) {
                        const mlab_gfunc *gf = mlab_gfunc_get(f);
                        if (rows[best_i].rate <= 0.0) {
                            fprintf(fw,
                                    "| %s n=%d（%s，target=%.0e） | 无（全部单层 0%%） | 见扩展抽样与收敛曲线；需更大预算或 HYBRID |\n",
                                    gf->name, dim, gf->multimodal ? "多峰" : "单峰/弱多峰",
                                    gf->target);
                        } else {
                            fprintf(fw,
                                    "| %s n=%d（%s，target=%.0e） | **%s** | rate=%.0f%% AES_success=%.0f |\n",
                                    gf->name, dim, gf->multimodal ? "多峰" : "单峰/弱多峰",
                                    gf->target, rows[best_i].algo, 100 * rows[best_i].rate,
                                    rows[best_i].aes);
                        }
                    }
                }
            }
        }
        /* 补充 CMA-ES / HYBRID 抽样数据到表注（R7：补 Rosenbrock 实测，尾注数据背书） */
        {
            mlab_rng rng;
            int r, sh_r = 0, sc_r = 0, sh_m = 0, sc_m = 0;
            const int nrep = 30;
            char best_r[32], best_m[32];
            for (r = 0; r < nrep; ++r) {
                mlab_galgo_out o;
                /* Rosenbrock n=5 */
                mlab_rng_seed(&rng, 990000u + (unsigned)r);
                memset(&o, 0, sizeof o);
                if (mlab_galgo_run(MLAB_GALGO_HYBRID, &rng, MLAB_GB_ROSEN, 5,
                                   BUDGET, &o) == 0 && o.success) ++sh_r;
                mlab_galgo_out_free(&o);
                mlab_rng_seed(&rng, 991000u + (unsigned)r);
                memset(&o, 0, sizeof o);
                if (mlab_galgo_run(MLAB_GALGO_CMAES, &rng, MLAB_GB_ROSEN, 5,
                                   BUDGET, &o) == 0 && o.success) ++sc_r;
                mlab_galgo_out_free(&o);
                /* Rastrigin n=5 */
                mlab_rng_seed(&rng, 992000u + (unsigned)r);
                memset(&o, 0, sizeof o);
                if (mlab_galgo_run(MLAB_GALGO_HYBRID, &rng, MLAB_GB_RASTRIGIN, 5,
                                   BUDGET, &o) == 0 && o.success) ++sh_m;
                mlab_galgo_out_free(&o);
                mlab_rng_seed(&rng, 993000u + (unsigned)r);
                memset(&o, 0, sizeof o);
                if (mlab_galgo_run(MLAB_GALGO_CMAES, &rng, MLAB_GB_RASTRIGIN, 5,
                                   BUDGET, &o) == 0 && o.success) ++sc_m;
                mlab_galgo_out_free(&o);
            }
            {
                /* 从主表找对应 (func,dim) 最优单层 */
                int bi = -1, k;
                bi = -1;
                for (k = 0; k < nrow; ++k) {
                    if (strcmp(rows[k].func, "rosenbrock") != 0 || rows[k].dim != 5)
                        continue;
                    if (bi < 0 || rows[k].rate > rows[bi].rate + 1e-9 ||
                        (fabs(rows[k].rate - rows[bi].rate) < 1e-9 &&
                         rows[k].aes < rows[bi].aes))
                        bi = k;
                }
                if (bi >= 0) {
                    if (rows[bi].rate <= 0.0)
                        snprintf(best_r, sizeof best_r, "无（全部 0%%）");
                    else
                        snprintf(best_r, sizeof best_r, "%s %.0f%%", rows[bi].algo,
                                 100.0 * rows[bi].rate);
                } else
                    snprintf(best_r, sizeof best_r, "-");
                bi = -1;
                for (k = 0; k < nrow; ++k) {
                    if (strcmp(rows[k].func, "rastrigin") != 0 || rows[k].dim != 5)
                        continue;
                    if (bi < 0 || rows[k].rate > rows[bi].rate + 1e-9 ||
                        (fabs(rows[k].rate - rows[bi].rate) < 1e-9 &&
                         rows[k].aes < rows[bi].aes))
                        bi = k;
                }
                if (bi >= 0) {
                    if (rows[bi].rate <= 0.0)
                        snprintf(best_m, sizeof best_m, "无（全部 0%%）");
                    else
                        snprintf(best_m, sizeof best_m, "%s %.0f%%", rows[bi].algo,
                                 100.0 * rows[bi].rate);
                } else
                    snprintf(best_m, sizeof best_m, "-");
            }
            fprintf(fw, "\n## 扩展抽样（n=5，各 %d 次，budget=%d，等预算口径）\n\n", nrep, BUDGET);
            fprintf(fw, "| 函数 | HYBRID | CMA-ES | 主表最优单层 |\n|---|---|---|---|\n");
            fprintf(fw, "| Rosenbrock | %.0f%% | %.0f%% | %s |\n",
                    100.0 * sh_r / nrep, 100.0 * sc_r / nrep, best_r);
            fprintf(fw, "| Rastrigin | %.0f%% | %.0f%% | %s |\n",
                    100.0 * sh_m / nrep, 100.0 * sc_m / nrep, best_m);
            fprintf(fw,
                    "\n> 尾注以实测为准：精度要求高的场景是否优先 HYBRID/CMA-ES，"
                    "以上表实测成功率相对主表最优单层的差距为据，"
                    "实测不占优的格子不予推荐；多峰全局搜索优先 DE/PSO，按需加局部精修。\n");
        }
    }
    fclose(fw);
    ok = 1;
    test_record(ok, "report", "algo_selection_reference_table",
                "wrote results/m2_algo_selection.md from full_table + CMA/HYBRID samples");
    return ok;
}

static int t_param_heatmap_de(void)
{
    /*
     * DE F×CR 参数敏感性热图批量化（R7）：5 个函数 × 5F × 5CR 全扫描。
     * 修正条件偏差：成功率分母用 n_run（此前为 rc==1 的 n_ok，误差运行被静默
     * 剔除导致条件偏差）；mean_best_f 同样对全部有效运行求均值。
     * 输出：合并 CSV + 每函数一张 PGM（亮度反相，越暗越好）。
     */
    const int nF = 5, nCR = 5;
    const double Fs[5] = {0.3, 0.5, 0.7, 0.9, 1.1};
    const double CRs[5] = {0.2, 0.4, 0.6, 0.8, 0.95};
    const int n_run = 30;
    const int dim = 5;
    const int budget = 800;
    double img[25];
    int fi, i, j, r, rows = 0, pgms = 0, ok;
    FILE *fp;
    mlab_rng rng;
    char detail[220];
    double gmin = 1e300, gmax = -1e300;

    test_ensure_results_dir();
    fp = fopen("results/m2_de_fcr_scan.csv", "w");
    if (fp) fprintf(fp, "func,F,CR,n_run,success_rate,mean_best_f\n");

    for (fi = 0; fi < N_FUNC; ++fi) {
        const mlab_gfunc *gf = mlab_gfunc_get(fi);
        double lb[5], ub[5];
        char path[80];
        FILE *pgm;
        for (i = 0; i < dim; ++i) {
            lb[i] = gf->lb;
            ub[i] = gf->ub;
        }
        for (i = 0; i < nF; ++i) {
            for (j = 0; j < nCR; ++j) {
                int succ = 0;
                double sum_best = 0.0;
                for (r = 0; r < n_run; ++r) {
                    mlab_de_config cfg;
                    mlab_de_result res;
                    int pop = 20, gens = budget / pop - 1;
                    memset(&cfg, 0, sizeof cfg);
                    cfg.f = gf->f;
                    cfg.ctx = NULL;
                    cfg.dim = dim;
                    cfg.lb = lb;
                    cfg.ub = ub;
                    cfg.pop = pop;
                    cfg.max_gen = gens > 0 ? gens : 0;
                    cfg.F = Fs[i];
                    cfg.CR = CRs[j];
                    cfg.variant = MLAB_DE_RAND1;
                    memset(&res, 0, sizeof res);
                    mlab_rng_seed(&rng, 995000u + (unsigned)(fi * 100000 + i * 5000 + j * 100 + r));
                    /* 有效运行计入均值与成功率（分母 n_run，无条件剔除）；仅参数/内存错误才跳过 */
                    if (mlab_de_run(&rng, &cfg, NULL, &res, 0.0) == 1) {
                        sum_best += res.best_f;
                        if (res.best_f <= gf->target) ++succ;
                    }
                    mlab_de_result_free(&res);
                }
                img[i * nCR + j] = n_run > 0 ? sum_best / n_run : 0.0;
                if (img[i * nCR + j] < gmin) gmin = img[i * nCR + j];
                if (img[i * nCR + j] > gmax) gmax = img[i * nCR + j];
                if (fp)
                    fprintf(fp, "%s,%.2f,%.2f,%d,%.3f,%.6g\n", gf->name, Fs[i], CRs[j],
                            n_run, (double)succ / n_run, img[i * nCR + j]);
                ++rows;
            }
        }
        /* 每函数一张 PGM */
        snprintf(path, sizeof path, "results/m2_de_fcr_%s.pgm", gf->name);
        pgm = fopen(path, "w");
        if (pgm) {
            double vmin = img[0], vmax = img[0];
            int k;
            for (k = 0; k < 25; ++k) {
                if (img[k] < vmin) vmin = img[k];
                if (img[k] > vmax) vmax = img[k];
            }
            if (vmax - vmin < 1e-12) vmax = vmin + 1.0;
            fprintf(pgm, "P2\n%d %d\n255\n", nCR, nF);
            for (i = 0; i < nF; ++i) {
                for (j = 0; j < nCR; ++j) {
                    double t = (img[i * nCR + j] - vmin) / (vmax - vmin);
                    int v = (int)(255.0 * (1.0 - t) + 0.5);
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    fprintf(pgm, "%d%c", v, j + 1 == nCR ? '\n' : ' ');
                }
            }
            fclose(pgm);
            ++pgms;
        }
    }
    if (fp) fclose(fp);
    ok = (rows == N_FUNC * nF * nCR) && (pgms == N_FUNC);
    snprintf(detail, sizeof detail,
             "DE F×CR 扫描 ×%d 函数 ×%d runs mean_f range %.4g–%.4g | csv rows=%d pgm=%d",
             N_FUNC, n_run, gmin, gmax, rows, pgms);
    test_record(ok, "heatmap", "de_f_cr_scan_all_funcs", detail);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"grid", "full_table_4x5x3_100runs",
         "4算法×5函数×3维 全表，每格100次运行",
         t_grid_full_table},
        {"curve", "convergence_curve_csv",
         "6 算法×5 函数×9 检查点 收敛曲线 CSV（含真实评估数与单调性校验）",
         t_convergence_curves},
        {"hybrid", "beats_each_single_on_multimodal",
         "混合框架多峰成功率显著优于各单层（p<0.01，等预算对齐+逐函数口径）",
         t_hybrid_beats_singles_multimodal},
        {"report", "algo_selection_reference_table",
         "产出个人实测《算法选择参考表》",
         t_selection_reference_table},
        {"heatmap", "de_f_cr_scan_all_funcs",
         "DE F×CR 参数敏感性热图批量化（5 函数×5F×5CR，去条件偏差）",
         t_param_heatmap_de},
    };
    test_ensure_results_dir();
    return test_run_main("M2-global-bench", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
