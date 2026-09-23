/*
 * B2: optcore — CG / Newton / BFGS / L-BFGS / DFP / Dogleg / NM / HJ / CD / DIRECT 多场景
 * 全量运行（无参数）时重算 results 目录下的 CSV（确定性可复现）。
 */
#include "harness.h"
#include "vec.h"
#include "linalg.h"
#include "opt.h"
#include "optcore.h"
#include "bench.h"
#include "conv.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double f_rosen(const double *x, int n, void *ctx)
{
    return mlab_rosenbrock(x, n, ctx);
}
static void g_rosen(const double *x, int n, double *g, void *ctx)
{
    mlab_rosenbrock_grad(x, n, g, ctx);
}
static void h_rosen(const double *x, int n, double *H, void *ctx)
{
    mlab_rosenbrock_hess(x, n, H, ctx);
}

static double f_quad(const double *x, int n, void *ctx)
{
    return mlab_quad(x, n, ctx);
}
static void g_quad(const double *x, int n, double *g, void *ctx)
{
    mlab_quad_grad(x, n, g, ctx);
}
static void h_quad(const double *x, int n, double *H, void *ctx)
{
    mlab_quad_hess(x, n, H, ctx);
}

static double f_sphere(const double *x, int n, void *ctx)
{
    return mlab_sphere(x, n, ctx);
}

/* ---- CG ---- */

static int t_cg_finite_termination(void)
{
    const int n = 12;
    double *evals = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    double *b = malloc(sizeof(double) * (size_t)n);
    double *x = calloc((size_t)n, sizeof(double));
    int i, it = 0, ok = 0;
    double res = 0;
    if (!evals || !A || !b || !x) goto done;
    for (i = 0; i < n; ++i) evals[i] = 1.0 + 9.0 * i / (n - 1);
    if (mlab_spd_from_spectrum(A, n, evals, 2) != 0) goto done;
    for (i = 0; i < n; ++i) b[i] = 1.0 + 0.1 * i;
    if (mlab_cg_solve(A, n, b, x, n + 5, 1e-12, &it, &res) != 0) goto done;
    ok = it <= n && res < 1e-10;
    {
        char d[80];
        snprintf(d, sizeof d, "n=%d iters=%d res=%.3e", n, it, res);
        test_record(ok, "cg", "finite_termination_spd", d);
    }
done:
    free(evals); free(A); free(b); free(x);
    return ok;
}

static int t_cg_rhs_zero(void)
{
    const int n = 5;
    double A[25], b[5] = {0}, x[5] = {1, 1, 1, 1, 1};
    double *evals = malloc(5 * sizeof(double));
    int it = 0, i, ok = 0;
    double res = 1;
    if (!evals) return 0;
    for (i = 0; i < 5; ++i) evals[i] = 2.0 + i;
    if (mlab_spd_from_spectrum(A, n, evals, 1) != 0) { free(evals); return 0; }
    mlab_cg_solve(A, n, b, x, 20, 1e-14, &it, &res);
    ok = mlab_nrm2(x, n) < 1e-10;
    {
        char d[64];
        snprintf(d, sizeof d, "||x||=%.3e iters=%d", mlab_nrm2(x, n), it);
        test_record(ok, "cg", "zero_rhs_solution_zero", d);
    }
    free(evals);
    return ok;
}

/* ---- 收敛阶实测（路线图 L217：牛顿 2±0.3；BFGS>1.2） ----
 * 用 run 的 f_hist：f ~ C·e²，故相邻 f 的阶估计 = 误差阶。
 * p_k = log(f_{k+1}/f_k)/log(f_k/f_{k-1})，只取噪声层之上的下降段三元的均值。 */

/* 只用最后 3 个有效三元（渐近段）的均值，避免非渐近相拉低阶估计 */
static double order_from_fhist(const double *fh, int len)
{
    double ps[64];
    int cnt = 0, k;
    for (k = 1; k + 1 < len && cnt < 64; ++k) {
        double fm1 = fh[k - 1], f0 = fh[k], f1 = fh[k + 1];
        double p;
        if (!(fm1 > 1e-24 && f0 > 1e-24 && f1 > 1e-24)) continue; /* 避开噪声层 */
        if (!(fm1 > f0 && f0 > f1)) continue;                      /* 严格下降段 */
        p = mlab_est_order_pair(fm1, f0, f1);
        if (p > 0.05 && p < 6.0)
            ps[cnt++] = p;
    }
    {
        int take = cnt < 3 ? cnt : 3; /* 最后 3 个有效估计 */
        double s = 0;
        int i;
        if (take == 0) return -1.0;
        for (i = cnt - take; i < cnt; ++i) s += ps[i];
        return s / take;
    }
}

static double g_order_newton, g_order_bfgs;

static int t_newton_rosen_converges(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[2] = {-1.2, 1.0};
    double fh[100];
    int ok;
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = h_rosen; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&run, fh, 100);
    mlab_opt_newton(&obj, x, 1e-12, 50, &run);
    g_order_newton = order_from_fhist(fh, run.iters < 100 ? run.iters : 100);
    ok = run.status == 0 && g_order_newton > 1.7 && g_order_newton < 2.3;
    {
        double f = obj.f(x, 2, NULL);
        char d[112];
        snprintf(d, sizeof d, "f=%.3e iters=%d 收敛阶=%.3f (expect 2±0.3)",
                 f, run.iters, g_order_newton);
        test_record(ok, "newton", "rosenbrock_convergence_order", d);
    }
    return ok;
}

static int t_newton_quadratic_one_step(void)
{
    /* SPD 二次型上牛顿一步精确收敛 */
    const int n = 5;
    double A[25], x[5];
    mlab_objective obj;
    mlab_quad_ctx qctx;
    mlab_opt_run run;
    double *evals = malloc(sizeof(double) * (size_t)n);
    int i, ok = 0;
    if (!evals) return 0;
    for (i = 0; i < n; ++i) evals[i] = 1.0 + (double)i;
    if (mlab_spd_from_spectrum(A, n, evals, 3) != 0) { free(evals); return 0; }
    free(evals);
    qctx.A = A;
    qctx.n = n;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = h_quad; obj.dim = n; obj.ctx = &qctx;
    for (i = 0; i < n; ++i) x[i] = 0.4 * (i + 1);
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_newton(&obj, x, 1e-14, 20, &run);
    ok = run.status == 0 && run.iters <= 2 && obj.f(x, n, &qctx) < 1e-24;
    {
        char d[80];
        snprintf(d, sizeof d, "iters=%d f=%.3e (SPD 二次一步精确)", run.iters, obj.f(x, n, &qctx));
        test_record(ok, "newton", "quadratic_one_step_exact", d);
    }
    return ok;
}

static int t_bfgs_rosen_superlinear(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[2] = {-1.2, 1.0};
    double fh[400];
    int ok;
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&run, fh, 400);
    mlab_opt_bfgs(&obj, x, 1e-14, 400, &run);
    g_order_bfgs = order_from_fhist(fh, run.iters < 400 ? run.iters : 400);
    ok = run.status == 0 && g_order_bfgs > 1.2;
    {
        double f = obj.f(x, 2, NULL);
        char d[112];
        snprintf(d, sizeof d, "f=%.3e iters=%d fe=%d 收敛阶=%.3f (expect >1.2)",
                 f, run.iters, run.fevals, g_order_bfgs);
        test_record(ok, "bfgs", "rosenbrock_superlinear_order", d);
    }
    return ok;
}

static int t_bfgs_beats_gd_rosen(void)
{
    mlab_objective obj;
    mlab_opt_run rb, rg;
    double xb[2] = {-1.2, 1.0}, xg[2] = {-1.2, 1.0};
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&rb, NULL, 0);
    mlab_opt_run_init(&rg, NULL, 0);
    mlab_opt_bfgs(&obj, xb, 1e-8, 400, &rb);
    mlab_opt_gd(&obj, xg, 1e-8, 400, &rg);
    {
        double fb = obj.f(xb, 2, NULL);
        double fg = obj.f(xg, 2, NULL);
        /* 路线图 L218：BFGS 函数评估数显著少于最速下降（差距 ≥5 倍）。
           fevals 现为真实计数（Armijo 逐次累加）。 */
        int ok = fb < 1e-6 && fb < fg && rb.fevals * 5 <= rg.fevals;
        char d[112];
        snprintf(d, sizeof d, "BFGS fe=%d f=%.2e | GD fe=%d f=%.2e",
                 rb.fevals, fb, rg.fevals, fg);
        test_record(ok, "bfgs", "rosenbrock_vs_gd_fevals", d);
        return ok;
    }
}

static int t_lbfgs_rosen_converges(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[2] = {-1.2, 1.0};
    int ok;
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_lbfgs(&obj, x, 1e-10, 500, 5, &run);
    ok = run.status == 0 && obj.f(x, 2, NULL) < 1e-8;
    {
        char d[96];
        snprintf(d, sizeof d, "mem=5 f=%.3e iters=%d fe=%d",
                 obj.f(x, 2, NULL), run.iters, run.fevals);
        test_record(ok, "lbfgs", "rosenbrock_mem5", d);
    }
    return ok;
}

static int t_lbfgs_beats_gd_rosen(void)
{
    mlab_objective obj;
    mlab_opt_run rl, rg;
    double xl[2] = {-1.2, 1.0}, xg[2] = {-1.2, 1.0};
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&rl, NULL, 0);
    mlab_opt_run_init(&rg, NULL, 0);
    mlab_opt_lbfgs(&obj, xl, 1e-8, 500, 5, &rl);
    mlab_opt_gd(&obj, xg, 1e-8, 400, &rg);
    {
        double fl = obj.f(xl, 2, NULL);
        int ok = fl < 1e-6 && rl.fevals < rg.fevals;
        char d[112];
        snprintf(d, sizeof d, "L-BFGS fe=%d f=%.2e | GD fe=%d",
                 rl.fevals, fl, rg.fevals);
        test_record(ok, "lbfgs", "lbfgs_fewer_evals_than_gd", d);
        return ok;
    }
}

static int t_dfp_rosen_converges(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[2] = {-1.2, 1.0};
    int ok;
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_dfp(&obj, x, 1e-12, 500, &run);
    ok = run.status == 0 && obj.f(x, 2, NULL) < 1e-10;
    {
        char d[96];
        snprintf(d, sizeof d, "f=%.3e iters=%d fe=%d",
                 obj.f(x, 2, NULL), run.iters, run.fevals);
        test_record(ok, "dfp", "rosenbrock_converges", d);
    }
    return ok;
}

/* ---- 信赖域 vs 线搜索（路线图 L219：≥10 个病态起点，失败率对比可复现） ---- */

#define TR_STARTS 12
static int g_tr_fail, g_gd_fail;

static int t_tr_vs_gd_ill_starts(void)
{
    const int n = 10;
    double A[100];
    mlab_quad_ctx qctx;
    mlab_objective obj;
    mlab_rng rng;
    double *evals = malloc(sizeof(double) * (size_t)n);
    int s, i, ok = 0;
    FILE *fp;
    if (!evals) return 0;
    for (i = 0; i < n; ++i)
        evals[i] = 1.0 + (100.0 - 1.0) * (double)i / (n - 1);
    if (mlab_spd_from_spectrum(A, n, evals, 17) != 0) { free(evals); return 0; }
    free(evals);
    qctx.A = A;
    qctx.n = n;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = h_quad; obj.dim = n; obj.ctx = &qctx;
    g_tr_fail = g_gd_fail = 0;
    fp = fopen("results/tr_vs_gd.csv", "w");
    if (fp) fprintf(fp, "start_id,gd_status,gd_iters,gd_f,tr_status,tr_iters,tr_f\n");
    mlab_rng_seed(&rng, 1717ULL);
    for (s = 0; s < TR_STARTS; ++s) {
        double xg[10], xt[10];
        mlab_opt_run rg, rt;
        for (i = 0; i < n; ++i) {
            xg[i] = -2.0 + 4.0 * mlab_rng_uniform(&rng);
            xt[i] = xg[i];
        }
        mlab_opt_run_init(&rg, NULL, 0);
        mlab_opt_run_init(&rt, NULL, 0);
        mlab_opt_gd(&obj, xg, 1e-6, 300, &rg);
        mlab_opt_dogleg(&obj, xt, 1e-6, 300, 1.0, &rt);
        if (rg.status != 0) ++g_gd_fail;
        if (rt.status != 0) ++g_tr_fail;
        if (fp)
            fprintf(fp, "%d,%d,%d,%.3e,%d,%d,%.3e\n", s, rg.status, rg.iters,
                    obj.f(xg, n, &qctx), rt.status, rt.iters, obj.f(xt, n, &qctx));
    }
    if (fp) fclose(fp);
    /* 信赖域失败率严格低于纯线搜索（病态 κ=100、300 步预算下） */
    ok = g_tr_fail < g_gd_fail && (g_gd_fail - g_tr_fail) >= 5;
    {
        char d[96];
        snprintf(d, sizeof d, "%d starts: GD fail=%d vs Dogleg fail=%d (κ=100)",
                 TR_STARTS, g_gd_fail, g_tr_fail);
        test_record(ok, "trust_region", "dogleg_fewer_failures_than_gd", d);
    }
    return ok;
}

static int t_dogleg_hard_start(void)
{
    mlab_objective obj;
    mlab_opt_run rt, rl;
    double xt[2] = {-1.2, 1.0}, xl[2] = {-1.2, 1.0};
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = h_rosen; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&rt, NULL, 0);
    mlab_opt_run_init(&rl, NULL, 0);
    mlab_opt_dogleg(&obj, xt, 1e-10, 80, 0.5, &rt);
    obj.hess = NULL;
    mlab_opt_gd(&obj, xl, 1e-6, 80, &rl);
    {
        double ft = obj.f(xt, 2, NULL);
        double fl = obj.f(xl, 2, NULL);
        int ok = ft < 1e-6 && ft <= fl + 1e-12;
        char d[96];
        snprintf(d, sizeof d, "dogleg f=%.2e status=%d | gd f=%.2e iters=%d",
                 ft, rt.status, fl, rl.iters);
        test_record(ok, "dogleg", "classic_start_vs_gd", d);
        return ok;
    }
}

static int t_dogleg_small_delta(void)
{
    /* 极小初始半径：信赖域子问题退化为 Cauchy 步，但半径应自动增长并收敛 */
    mlab_objective obj;
    mlab_opt_run rt;
    double xt[2] = {-1.2, 1.0};
    obj.f = f_rosen; obj.grad = g_rosen; obj.hess = h_rosen; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&rt, NULL, 0);
    mlab_opt_dogleg(&obj, xt, 1e-10, 500, 1e-3, &rt);
    {
        double ft = obj.f(xt, 2, NULL);
        int ok = ft < 1e-6;
        char d[96];
        snprintf(d, sizeof d, "δ0=1e-3 → f=%.2e iters=%d status=%d", ft, rt.iters, rt.status);
        test_record(ok, "dogleg", "small_delta_recover", d);
        return ok;
    }
}

/* ---- Nelder-Mead（含噪声韧性，路线图 L220） ---- */

static int t_neldermead_sphere(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[4] = {2, -1, 0.5, 3};
    obj.f = f_sphere; obj.grad = NULL; obj.hess = NULL; obj.dim = 4; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_neldermead(&obj, x, 1e-10, 400, &run);
    {
        double f = obj.f(x, 4, NULL);
        int ok = f < 1e-8;
        char d[64];
        snprintf(d, sizeof d, "sphere f=%.3e iters=%d", f, run.iters);
        test_record(ok, "nelder_mead", "sphere_dim4", d);
        return ok;
    }
}

static int t_neldermead_rosen(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[2] = {-1.2, 1.0};
    obj.f = f_rosen; obj.grad = NULL; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_neldermead(&obj, x, 1e-8, 2000, &run);
    {
        double err = sqrt((x[0] - 1) * (x[0] - 1) + (x[1] - 1) * (x[1] - 1));
        double f = obj.f(x, 2, NULL);
        int ok = err < 0.2 || f < 1e-4; /* 无导数方法：接近即可 */
        char d[80];
        snprintf(d, sizeof d, "err=%.3e f=%.3e iters=%d", err, f, run.iters);
        test_record(ok, "nelder_mead", "rosenbrock_dim2", d);
        return ok;
    }
}

/* 噪声目标：f(x) + amp·U(-1,1)，确定性（固定 seed 的 rng 流） */
typedef struct {
    double amp;
    mlab_rng rng;
} noise_ctx;

static double g_noise_nm_err, g_noise_gd_err;

static double f_noisy_sphere(const double *x, int n, void *ctx)
{
    noise_ctx *c = ctx;
    return mlab_sphere(x, n, NULL) + c->amp * (2.0 * mlab_rng_uniform(&c->rng) - 1.0);
}

/* 噪声目标的中心差分梯度（每步消耗 2n 个噪声样本） */
static void g_noisy_sphere(const double *x, int n, double *g, void *ctx)
{
    noise_ctx *c = ctx;
    const double h = 1e-4;
    double *xp = malloc((size_t)n * sizeof(double));
    int j;
    if (!xp) return;
    for (j = 0; j < n; ++j) {
        double fp, fm;
        memcpy(xp, x, (size_t)n * sizeof(double));
        xp[j] += h;
        fp = f_noisy_sphere(xp, n, c);
        xp[j] = x[j] - h;
        fm = f_noisy_sphere(xp, n, c);
        g[j] = (fp - fm) / (2.0 * h);
    }
    free(xp);
}

static int t_neldermead_noise_robust(void)
{
    /* 路线图 L220 场景：加性噪声（幅度已知）下的无导数 vs 梯度法对照。
       目标 sphere(x) + amp·U(-1,1)，n=4，最小点在原点。 */
    const double amp = 0.05;
    const int n = 4;
    noise_ctx nc;
    mlab_objective obj;
    mlab_opt_run rn, rg;
    double xn[4] = {2, -1, 0.5, 3}, xg[4] = {2, -1, 0.5, 3};
    int ok;
    nc.amp = amp;
    mlab_rng_seed(&nc.rng, 4242ULL);
    obj.f = f_noisy_sphere; obj.grad = NULL; obj.hess = NULL; obj.dim = n; obj.ctx = &nc;
    mlab_opt_run_init(&rn, NULL, 0);
    mlab_opt_neldermead(&obj, xn, 1e-14, 4000, &rn);
    g_noise_nm_err = mlab_nrm2(xn, n);
    /* 梯度法对照：FD 梯度被噪声淹没（噪声/h ~ 1000 量级） */
    obj.grad = g_noisy_sphere;
    mlab_opt_run_init(&rg, NULL, 0);
    mlab_opt_gd(&obj, xg, 1e-8, 400, &rg);
    g_noise_gd_err = mlab_nrm2(xg, n);
    /* NM 终值误差 ≤ 3×噪声幅；梯度法同条件发散或显著劣化 */
    ok = g_noise_nm_err <= 3.0 * amp + 0.1 && g_noise_gd_err > g_noise_nm_err;
    {
        char d[128];
        snprintf(d, sizeof d, "amp=%.2f n=4: NM err=%.3f vs GD err=%.3f (NM best-ever)",
                 amp, g_noise_nm_err, g_noise_gd_err);
        test_record(ok, "nelder_mead", "noise_robust_vs_gradient", d);
    }
    return ok;
}

/* ---- 无导数/地图级新算法 ---- */

static int t_coord_descent_sphere(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[4] = {2, -1, 0.5, 3};
    obj.f = f_sphere; obj.grad = NULL; obj.hess = NULL; obj.dim = 4; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_coord_descent(&obj, x, 1e-14, 200, 0.5, &run);
    {
        double f = obj.f(x, 4, NULL);
        int ok = f < 1e-12 && run.status == 0;
        char d[80];
        snprintf(d, sizeof d, "f=%.3e sweeps=%d fe=%d", f, run.iters, run.fevals);
        test_record(ok, "coord_descent", "sphere_dim4", d);
        return ok;
    }
}

static int t_hooke_jeeves_sphere(void)
{
    mlab_objective obj;
    mlab_opt_run run;
    double x[4] = {2, -1, 0.5, 3};
    obj.f = f_sphere; obj.grad = NULL; obj.hess = NULL; obj.dim = 4; obj.ctx = NULL;
    mlab_opt_run_init(&run, NULL, 0);
    mlab_opt_hooke_jeeves(&obj, x, 1e-10, 500, 0.5, &run);
    {
        double f = obj.f(x, 4, NULL);
        int ok = f < 1e-10;
        char d[80];
        snprintf(d, sizeof d, "f=%.3e iters=%d fe=%d", f, run.iters, run.fevals);
        test_record(ok, "hooke_jeeves", "sphere_dim4", d);
        return ok;
    }
}

static int t_direct_sphere_box(void)
{
    mlab_objective obj;
    double lb[2] = {-5, -5}, ub[2] = {5, 5}, xb[2];
    double fbest = 1e300;
    int ne = 0, ok;
    obj.f = f_sphere; obj.grad = NULL; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_direct(&obj, lb, ub, 2, 400, 1e-3, xb, &fbest, &ne);
    ok = fbest < 1e-6;
    {
        char d[96];
        snprintf(d, sizeof d, "f=%.3e evals=%d (box [-5,5]^2)", fbest, ne);
        test_record(ok, "direct", "sphere_dim2_box", d);
        return ok;
    }
}

static int t_direct_rosen_box(void)
{
    mlab_objective obj;
    double lb[2] = {-5, -5}, ub[2] = {5, 5}, xb[2];
    double fbest = 1e300;
    int ne = 0, ok;
    obj.f = f_rosen; obj.grad = NULL; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_direct(&obj, lb, ub, 2, 12000, 1e-3, xb, &fbest, &ne);
    /* 判据：DIRECT 全局搜索能进入 Rosenbrock 谷底区域（f<1）。
       曲谷内的精修交给局部方法——这正是 M2 混合框架的动机（如实注明）。 */
    ok = fbest < 1.0;
    {
        char d[96];
        snprintf(d, sizeof d, "f=%.3e evals=%d (Rosenbrock box, 谷底附近)", fbest, ne);
        test_record(ok, "direct", "rosenbrock_dim2_box", d);
        return ok;
    }
}

static int t_bench_values(void)
{
    double x0[2] = {0, 0}, x1[2] = {1, 1};
    int ok = fabs(mlab_sphere(x0, 2, NULL)) < 1e-15 &&
             fabs(mlab_rosenbrock(x1, 2, NULL)) < 1e-15;
    char d[80];
    snprintf(d, sizeof d, "sphere(0)=%.1e rosen(1,1)=%.1e",
             mlab_sphere(x0, 2, NULL), mlab_rosenbrock(x1, 2, NULL));
    test_record(ok, "bench", "known_minima", d);
    return ok;
}

static int t_conv_helper_unused(void)
{
    double r[] = {1.0, 0.5, 0.25, 0.125};
    double rho = mlab_est_conv_factor(r, 4, 3);
    int ok = fabs(rho - 0.5) < 0.05;
    char d[48];
    snprintf(d, sizeof d, "rho=%.4f want 0.5", rho);
    test_record(ok, "conv", "est_factor_geometric", d);
    return ok;
}

/* ---- results CSV（全量运行时重算落盘） ---- */

static void write_csvs(void)
{
    FILE *fp = fopen("results/conv_order.csv", "w");
    if (fp) {
        fprintf(fp, "method,order_est,criterion\n");
        fprintf(fp, "newton_rosenbrock,%.4f,2±0.3\n", g_order_newton);
        fprintf(fp, "bfgs_rosenbrock,%.4f,>1.2\n", g_order_bfgs);
        fclose(fp);
    }
    fp = fopen("results/noise_nm.csv", "w");
    if (fp) {
        fprintf(fp, "amp,nm_err,gd_err,criterion\n");
        fprintf(fp, "0.05,%.6f,%.6f,nm<=3amp+0.1; gd劣化\n", g_noise_nm_err, g_noise_gd_err);
        fclose(fp);
    }
    /* tr_vs_gd.csv 已在 case 内落盘 */
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"cg", "finite_termination_spd", "CG 在 n 维 SPD 上有限步终止", t_cg_finite_termination},
        {"cg", "zero_rhs_solution_zero", "b=0 时解为 0", t_cg_rhs_zero},
        {"newton", "rosenbrock_convergence_order", "牛顿 Rosenbrock 收敛阶 2±0.3（L217）", t_newton_rosen_converges},
        {"newton", "quadratic_one_step_exact", "SPD 二次型牛顿一步精确", t_newton_quadratic_one_step},
        {"bfgs", "rosenbrock_superlinear_order", "BFGS 超线性证据（阶>1.2，L217）", t_bfgs_rosen_superlinear},
        {"bfgs", "rosenbrock_vs_gd_fevals", "BFGS 评估数（真实计数）少于 GD ≥5×（L218）", t_bfgs_beats_gd_rosen},
        {"lbfgs", "rosenbrock_mem5", "L-BFGS mem=5 收敛", t_lbfgs_rosen_converges},
        {"lbfgs", "lbfgs_fewer_evals_than_gd", "L-BFGS 评估数少于 GD", t_lbfgs_beats_gd_rosen},
        {"dfp", "rosenbrock_converges", "DFP 收敛 Rosenbrock", t_dfp_rosen_converges},
        {"trust_region", "dogleg_fewer_failures_than_gd", "12 病态起点失败率对比（L219）", t_tr_vs_gd_ill_starts},
        {"dogleg", "classic_start_vs_gd", "信赖域 dogleg 病态起点", t_dogleg_hard_start},
        {"dogleg", "small_delta_recover", "极小初始半径自动恢复", t_dogleg_small_delta},
        {"nelder_mead", "sphere_dim4", "NM 解 4 维球面", t_neldermead_sphere},
        {"nelder_mead", "rosenbrock_dim2", "NM 解 Rosenbrock", t_neldermead_rosen},
        {"nelder_mead", "noise_robust_vs_gradient", "加性噪声下 NM 韧性 vs 梯度法（L220）", t_neldermead_noise_robust},
        {"coord_descent", "sphere_dim4", "坐标下降解 4 维球面", t_coord_descent_sphere},
        {"hooke_jeeves", "sphere_dim4", "Hooke-Jeeves 解 4 维球面", t_hooke_jeeves_sphere},
        {"direct", "sphere_dim2_box", "DIRECT 盒约束球面", t_direct_sphere_box},
        {"direct", "rosenbrock_dim2_box", "DIRECT 盒约束 Rosenbrock", t_direct_rosen_box},
        {"bench", "known_minima", "基准函数已知最小值", t_bench_values},
        {"conv", "est_factor_geometric", "几何序列收敛因子估计", t_conv_helper_unused},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("B2-newton-quasi-tr", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
