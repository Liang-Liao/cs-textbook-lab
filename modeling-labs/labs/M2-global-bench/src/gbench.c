#include "gbench.h"
#include "bench.h"
#include "bench_sa.h"
#include "sa.h"
#include "de.h"
#include "pso.h"
#include "cmaes.h"
#include "optcore.h"
#include "opt.h"
#include "dist.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- 基准函数 ---------- */

static double g_griewank(const double *x, int n, void *ctx)
{
    double s = 0.0, p = 1.0;
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) {
        s += x[i] * x[i];
        p *= cos(x[i] / sqrt((double)i + 1.0));
    }
    return s / 4000.0 - p + 1.0;
}

static double g_ackley(const double *x, int n, void *ctx)
{
    double s1 = 0.0, s2 = 0.0;
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) {
        s1 += x[i] * x[i];
        s2 += cos(2.0 * M_PI * x[i]);
    }
    return -20.0 * exp(-0.2 * sqrt(s1 / n)) - exp(s2 / n) + 20.0 + exp(1.0);
}

static const mlab_gfunc g_funcs[MLAB_GB_NFUNCS] = {
    {MLAB_GB_SPHERE, "sphere", mlab_sphere, -5.12, 5.12, 0.0, 1e-4, 0},
    {MLAB_GB_ROSEN, "rosenbrock", mlab_rosenbrock, -2.0, 2.0, 0.0, 1e-2, 0},
    {MLAB_GB_RASTRIGIN, "rastrigin", mlab_rastrigin, -5.12, 5.12, 0.0, 0.05, 1},
    {MLAB_GB_GRIEWANK, "griewank", g_griewank, -6.0, 6.0, 0.0, 0.05, 1},
    {MLAB_GB_ACKLEY, "ackley", g_ackley, -5.0, 5.0, 0.0, 0.05, 1}
};

const mlab_gfunc *mlab_gfunc_get(int id)
{
    if (id < 0 || id >= MLAB_GB_NFUNCS) return NULL;
    return &g_funcs[id];
}

static const char *g_algonames[MLAB_GALGO_NALGOS] = {
    "SA", "GA", "DE", "PSO", "CMAES", "HYBRID"
};

const char *mlab_galgo_name(int algo)
{
    if (algo < 0 || algo >= MLAB_GALGO_NALGOS) return "?";
    return g_algonames[algo];
}

void mlab_galgo_out_free(mlab_galgo_out *o)
{
    if (!o) return;
    free(o->best_x);
    o->best_x = NULL;
}

static void fill_bounds(const mlab_gfunc *g, int dim, double *lb, double *ub)
{
    int i;
    for (i = 0; i < dim; ++i) {
        lb[i] = g->lb;
        ub[i] = g->ub;
    }
}

/* 实数编码 GA：锦标赛 + 算术交叉 + 高斯变异 */
int mlab_real_ga_run(mlab_rng *rng,
                     double (*f)(const double *, int, void *), void *ctx,
                     int dim, const double *lb, const double *ub,
                     int pop, int budget,
                     double *best_f, double *best_x, int *n_eval)
{
    double *P = NULL, *C = NULL, *fit = NULL, *fitc = NULL;
    int i, j, evals = 0;
    double bf = 1e300;
    if (!rng || !f || dim <= 0 || pop < 4 || !best_f) return -1;
    P = (double *)malloc((size_t)pop * (size_t)dim * sizeof(double));
    C = (double *)malloc((size_t)pop * (size_t)dim * sizeof(double));
    fit = (double *)malloc((size_t)pop * sizeof(double));
    fitc = (double *)malloc((size_t)pop * sizeof(double));
    if (!P || !C || !fit || !fitc) {
        free(P); free(C); free(fit); free(fitc);
        return -1;
    }
    for (i = 0; i < pop; ++i) {
        for (j = 0; j < dim; ++j)
            P[i * dim + j] = lb[j] + (ub[j] - lb[j]) * mlab_rng_uniform(rng);
        fit[i] = f(&P[i * dim], dim, ctx);
        ++evals;
        if (fit[i] < bf) {
            bf = fit[i];
            if (best_x) memcpy(best_x, &P[i * dim], (size_t)dim * sizeof(double));
        }
    }
    while (evals < budget) {
        /* 子代数钳制到剩余预算：同预算对比下 GA 评估数恰好 = budget */
        int nchild = pop;
        if (evals + nchild > budget) nchild = budget - evals;
        if (nchild <= 0) break;
        for (i = 0; i < nchild; ++i) {
            int p1 = (int)(mlab_rng_uniform(rng) * pop);
            int p2 = (int)(mlab_rng_uniform(rng) * pop);
            int t1 = (int)(mlab_rng_uniform(rng) * pop);
            int t2 = (int)(mlab_rng_uniform(rng) * pop);
            if (p1 >= pop) p1 = pop - 1;
            if (p2 >= pop) p2 = pop - 1;
            if (t1 >= pop) t1 = pop - 1;
            if (t2 >= pop) t2 = pop - 1;
            /* 锦标赛选父（最小化） */
            p1 = (fit[t1] <= fit[t2]) ? t1 : t2;
            t1 = (int)(mlab_rng_uniform(rng) * pop);
            t2 = (int)(mlab_rng_uniform(rng) * pop);
            if (t1 >= pop) t1 = pop - 1;
            if (t2 >= pop) t2 = pop - 1;
            p2 = (fit[t1] <= fit[t2]) ? t1 : t2;
            for (j = 0; j < dim; ++j) {
                double w = 0.7; /* 算术交叉向 p1 偏 */
                double v = w * P[p1 * dim + j] + (1.0 - w) * P[p2 * dim + j];
                /* 变异 */
                if (mlab_rng_uniform(rng) < 0.25) {
                    double range = ub[j] - lb[j];
                    v += 0.08 * range * mlab_rng_normal(rng);
                }
                if (v < lb[j]) v = lb[j];
                if (v > ub[j]) v = ub[j];
                C[i * dim + j] = v;
            }
            fitc[i] = f(&C[i * dim], dim, ctx);
            ++evals;
            if (fitc[i] < bf) {
                bf = fitc[i];
                if (best_x) memcpy(best_x, &C[i * dim], (size_t)dim * sizeof(double));
            }
        }
        /* 精英：子代与同序位父代逐位竞争（仅已有子代参与，避免读未初始化 fitc） */
        for (i = 0; i < nchild; ++i) {
            if (fitc[i] <= fit[i]) {
                memcpy(&P[i * dim], &C[i * dim], (size_t)dim * sizeof(double));
                fit[i] = fitc[i];
            }
        }
    }
    *best_f = bf;
    if (n_eval) *n_eval = evals;
    free(P); free(C); free(fit); free(fitc);
    return 0;
}

double mlab_two_prop_p_one_sided(int s1, int n1, int s2, int n2)
{
    double p1, p2, pp, se, z;
    if (n1 <= 0 || n2 <= 0) return 1.0;
    p1 = (double)s1 / (double)n1;
    p2 = (double)s2 / (double)n2;
    pp = (double)(s1 + s2) / (double)(n1 + n2);
    se = sqrt(pp * (1.0 - pp) * (1.0 / n1 + 1.0 / n2));
    if (se <= 0.0) return p1 > p2 ? 0.0 : 1.0;
    z = (p1 - p2) / se;
    return 1.0 - mlab_normal_cdf(z, 0.0, 1.0);
}

static int ok_from_vendor(int rc, int n_eval)
{
    /* C 层约定：成功返回 1，失败返回 0；本 lab 统一 0=成功 */
    if (rc == 1) return 0;
    if (rc == 0 && n_eval > 0) return 0;
    return -1;
}

static int run_sa(mlab_rng *rng, const mlab_gfunc *g, int dim, int budget,
                  double *bf, double *bx, int *ne, int *evals_to_t)
{
    mlab_sa_problem prob;
    mlab_sa_config cfg;
    mlab_sa_result res;
    double *lb = (double *)malloc((size_t)dim * sizeof(double));
    double *ub = (double *)malloc((size_t)dim * sizeof(double));
    double *x0 = (double *)malloc((size_t)dim * sizeof(double));
    int i, rc;
    if (!lb || !ub || !x0) {
        free(lb); free(ub); free(x0);
        return -1;
    }
    fill_bounds(g, dim, lb, ub);
    for (i = 0; i < dim; ++i)
        x0[i] = lb[i] + (ub[i] - lb[i]) * mlab_rng_uniform(rng);
    memset(&prob, 0, sizeof prob);
    prob.dim = dim;
    prob.f = g->f;
    prob.ctx = NULL;
    prob.lb = lb;
    prob.ub = ub;
    memset(&cfg, 0, sizeof cfg);
    /* 由预算反推：fevals ≈ inner * n_outer（+T0 标定耗）；真实 n_fevals 随结果返回 */
    cfg.inner = 20;
    cfg.alpha = 0.90;
    cfg.T_min = 1e-4;
    cfg.step_scale = 0.35;
    cfg.calibrate_T0 = 1;
    cfg.target_accept = 0.8;
    cfg.max_outer = budget / cfg.inner;
    if (cfg.max_outer < 1) cfg.max_outer = 1;
    cfg.step_fixed = 0;
    memset(&res, 0, sizeof res);
    rc = mlab_sa_continuous(rng, &prob, &cfg, x0, &res);
    *bf = res.f_best;
    *ne = res.n_fevals;
    if (bx && res.x_best) memcpy(bx, res.x_best, (size_t)dim * sizeof(double));
    *evals_to_t = (*bf <= g->target) ? res.n_fevals : -1;
    rc = ok_from_vendor(rc, *ne);
    mlab_sa_result_free(&res);
    free(lb); free(ub); free(x0);
    return rc;
}

static int run_de(mlab_rng *rng, const mlab_gfunc *g, int dim, int budget,
                  double *bf, double *bx, int *ne, int *evals_to_t)
{
    mlab_de_config cfg;
    mlab_de_result res;
    double *lb = (double *)malloc((size_t)dim * sizeof(double));
    double *ub = (double *)malloc((size_t)dim * sizeof(double));
    int pop = 20, gens;
    int rc;
    if (!lb || !ub) {
        free(lb); free(ub);
        return -1;
    }
    fill_bounds(g, dim, lb, ub);
    /* C5 修后口径：父代 fit 缓存，n_eval = pop*(gens+1) = budget（budget 可被 pop 整除时） */
    gens = budget / pop - 1;
    if (gens < 0) gens = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.f = g->f;
    cfg.ctx = NULL;
    cfg.dim = dim;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.pop = pop;
    cfg.max_gen = gens;
    cfg.F = 0.6;
    cfg.CR = 0.7;
    cfg.variant = MLAB_DE_RAND1;
    memset(&res, 0, sizeof res);
    rc = mlab_de_run(rng, &cfg, NULL, &res, 0.0);
    *bf = res.best_f;
    *ne = res.n_eval;
    if (bx && res.best_x) memcpy(bx, res.best_x, (size_t)dim * sizeof(double));
    *evals_to_t = (*bf <= g->target) ? res.n_eval : -1;
    rc = ok_from_vendor(rc, *ne);
    mlab_de_result_free(&res);
    free(lb); free(ub);
    return rc;
}

static int run_pso(mlab_rng *rng, const mlab_gfunc *g, int dim, int budget,
                   double *bf, double *bx, int *ne, int *evals_to_t)
{
    mlab_pso_config cfg;
    mlab_pso_result res;
    double *lb = (double *)malloc((size_t)dim * sizeof(double));
    double *ub = (double *)malloc((size_t)dim * sizeof(double));
    int swarm = 20, gens;
    int rc;
    if (!lb || !ub) {
        free(lb); free(ub);
        return -1;
    }
    fill_bounds(g, dim, lb, ub);
    gens = budget / swarm - 1;
    if (gens < 0) gens = 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.f = g->f;
    cfg.ctx = NULL;
    cfg.dim = dim;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.swarm = swarm;
    cfg.max_gen = gens;
    cfg.w = 0.72;
    cfg.w_linear = 1;
    cfg.w_end = 0.4;
    cfg.c1 = 1.49;
    cfg.c2 = 1.49;
    cfg.vmax_scale = 0.2;
    cfg.topo = MLAB_PSO_TOPO_GLOBAL;
    cfg.mode = MLAB_PSO_MODE_FULL;
    cfg.stop_f = g->target;
    memset(&res, 0, sizeof res);
    rc = mlab_pso_run(rng, &cfg, NULL, &res);
    *bf = res.best_f;
    *ne = res.n_eval;
    if (bx && res.best_x) memcpy(bx, res.best_x, (size_t)dim * sizeof(double));
    *evals_to_t = (*bf <= g->target) ? res.n_eval : -1;
    rc = ok_from_vendor(rc, *ne);
    mlab_pso_result_free(&res);
    free(lb); free(ub);
    return rc;
}

static int run_cmaes(mlab_rng *rng, const mlab_gfunc *g, int dim, int budget,
                     double *bf, double *bx, int *ne, int *evals_to_t)
{
    mlab_cmaes_config cfg;
    mlab_cmaes_result res;
    double *lb = (double *)malloc((size_t)dim * sizeof(double));
    double *ub = (double *)malloc((size_t)dim * sizeof(double));
    int rc;
    if (!lb || !ub) {
        free(lb); free(ub);
        return -1;
    }
    fill_bounds(g, dim, lb, ub);
    memset(&cfg, 0, sizeof cfg);
    cfg.f = g->f;
    cfg.ctx = NULL;
    cfg.dim = dim;
    cfg.lb = lb;
    cfg.ub = ub;
    cfg.max_evals = budget;
    cfg.sigma0 = 0.25 * (g->ub - g->lb);
    cfg.stop_f = g->target;
    cfg.x0_scale = 0.3 * (g->ub - g->lb);
    cfg.track_angle = 0;
    memset(&res, 0, sizeof res);
    rc = mlab_cmaes_run(rng, &cfg, NULL, &res);
    *bf = res.best_f;
    *ne = res.n_eval;
    if (bx && res.best_x) memcpy(bx, res.best_x, (size_t)dim * sizeof(double));
    *evals_to_t = (*bf <= g->target) ? res.n_eval : -1;
    rc = ok_from_vendor(rc, *ne);
    mlab_cmaes_result_free(&res);
    free(lb); free(ub);
    return rc;
}

int mlab_galgo_run(int algo, mlab_rng *rng, int func_id, int dim, int budget,
                   mlab_galgo_out *out)
{
    const mlab_gfunc *g = mlab_gfunc_get(func_id);
    double *bx = NULL;
    double bf = 1e300;
    int ne = 0, ett = -1, rc = -1;
    if (!g || !rng || !out || dim < 1) return -1;
    memset(out, 0, sizeof *out);
    out->evals_to_target = -1;

    if (algo == MLAB_GALGO_HYBRID)
        return mlab_hybrid_run(rng, func_id, dim, budget, MLAB_GALGO_PSO, out);

    bx = (double *)malloc((size_t)dim * sizeof(double));
    if (!bx) return -1;

    if (algo == MLAB_GALGO_SA) {
        rc = run_sa(rng, g, dim, budget, &bf, bx, &ne, &ett);
    } else if (algo == MLAB_GALGO_GA) {
        double *lb = (double *)malloc((size_t)dim * sizeof(double));
        double *ub = (double *)malloc((size_t)dim * sizeof(double));
        if (lb && ub) {
            fill_bounds(g, dim, lb, ub);
            rc = mlab_real_ga_run(rng, g->f, NULL, dim, lb, ub,
                                  30, budget, &bf, bx, &ne);
            ett = (bf <= g->target) ? ne : -1;
        } else {
            rc = -1;
        }
        free(lb);
        free(ub);
    } else if (algo == MLAB_GALGO_DE) {
        rc = run_de(rng, g, dim, budget, &bf, bx, &ne, &ett);
    } else if (algo == MLAB_GALGO_PSO) {
        rc = run_pso(rng, g, dim, budget, &bf, bx, &ne, &ett);
    } else if (algo == MLAB_GALGO_CMAES) {
        rc = run_cmaes(rng, g, dim, budget, &bf, bx, &ne, &ett);
    } else {
        rc = -1;
    }

    out->best_f = bf;
    out->n_eval = ne;
    out->success = (bf <= g->target);
    out->evals_to_target = ett;
    free(out->best_x);
    out->best_x = bx;
    return rc;
}

int mlab_hybrid_run(mlab_rng *rng, int func_id, int dim, int budget,
                    int global_algo, mlab_galgo_out *out)
{
    const mlab_gfunc *g = mlab_gfunc_get(func_id);
    mlab_galgo_out gout;
    mlab_objective obj;
    mlab_opt_run run;
    double *x = NULL;
    double fcur, step0;
    int global_budget, local_budget, rc;
    int evals_total, hj_iters, i;
    double *x_pre = NULL;

    if (!g || !rng || !out || dim < 1) return -1;
    if (global_algo < 0) global_algo = MLAB_GALGO_PSO;
    /*
     * 混合框架（R7 等预算对齐）：总评估预算 = budget，与单层算法同口径。
     * 分配：全局搜索 75%（PSO 命中 target 提前停时剩余预算流转给精修），
     * 局部精修 25% + 全局剩余：Hooke-Jeeves 模式搜索（step0=0.1×盒宽，
     * 失败减半，只采纳改进）。n_eval 全程真实计数。
     */
    local_budget = budget / 4;
    global_budget = budget - local_budget;

    memset(&gout, 0, sizeof gout);
    rc = mlab_galgo_run(global_algo, rng, func_id, dim, global_budget, &gout);
    if (rc != 0 && gout.best_x == NULL) {
        mlab_galgo_out_free(&gout);
        return -1;
    }
    evals_total = gout.n_eval;
    x = (double *)malloc((size_t)dim * sizeof(double));
    x_pre = (double *)malloc((size_t)dim * sizeof(double));
    if (!x || !x_pre) {
        free(x);
        free(x_pre);
        mlab_galgo_out_free(&gout);
        return -1;
    }
    if (gout.best_x) {
        memcpy(x, gout.best_x, (size_t)dim * sizeof(double));
        fcur = gout.best_f;   /* 与 best_x 同点，无需重求值 */
    } else {
        for (i = 0; i < dim; ++i)
            x[i] = g->lb + (g->ub - g->lb) * mlab_rng_uniform(rng);
        fcur = g->f(x, dim, NULL);
        ++evals_total;
    }

    /* 局部精修：Hooke-Jeeves，剩余预算按每迭代约 2·dim+2 次求值折算迭代数。
     * HJ 只采纳改进（含初始点求值计入 run.fevals），结果不劣于起点。 */
    hj_iters = (local_budget + (global_budget - (int)gout.n_eval)) / (2 * dim + 2);
    if (hj_iters < 5) hj_iters = 5;
    if (hj_iters > 2000) hj_iters = 2000;
    step0 = 0.1 * (g->ub - g->lb);
    memset(&obj, 0, sizeof obj);
    obj.dim = dim;
    obj.f = g->f;
    obj.grad = NULL;
    obj.hess = NULL;
    obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    memcpy(x_pre, x, (size_t)dim * sizeof(double));
    mlab_opt_hooke_jeeves(&obj, x, 1e-9, hj_iters, step0, &run);
    evals_total += run.fevals;
    fcur = run.f_final;
    for (i = 0; i < dim; ++i) {
        if (x[i] < g->lb) x[i] = g->lb;
        if (x[i] > g->ub) x[i] = g->ub;
    }
    {
        /* 盒内裁剪后真求值一次：裁剪无益则保留 HJ 原点 */
        double f_check = g->f(x, dim, NULL);
        ++evals_total;
        if (f_check < fcur) {
            fcur = f_check;
        } else {
            memcpy(x, x_pre, (size_t)dim * sizeof(double));
        }
    }

    memset(out, 0, sizeof *out);
    out->best_f = fcur;
    out->n_eval = evals_total;
    out->success = (out->best_f <= g->target);
    out->evals_to_target = out->success ? out->n_eval : -1;
    out->best_x = x;
    free(x_pre);
    mlab_galgo_out_free(&gout);
    return 0;
}
