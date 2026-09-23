/*
 * B1: linesearch (golden / armijo / wolfe / steepest descent)
 * 全量运行（无参数）时重算 results/*.csv（确定性可复现）。
 */
#include "harness.h"
#include "vec.h"
#include "linalg.h"
#include "linesearch.h"
#include "opt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHI 0.6180339887498949

typedef struct { double a, b, c; } quad_ctx; /* 0.5 a x^2 + b x + c */

static double parabola(double alpha, void *ctx)
{
    quad_ctx *q = ctx;
    return 0.5 * q->a * alpha * alpha + q->b * alpha + q->c;
}

static int t_golden_parabola_min(void)
{
    quad_ctx q = {2.0, -2.8, 1.0}; /* min at alpha=1.4 */
    double fmin = 0;
    double x = mlab_golden_section(parabola, &q, 0.0, 4.0, 80, 1e-10, &fmin, NULL);
    int ok = fabs(x - 1.4) < 1e-6;
    char d[80];
    snprintf(d, sizeof d, "x*=%.8f want 1.4", x);
    test_record(ok, "linesearch", "golden_parabola_minimum", d);
    return ok;
}

static int t_golden_zero_min(void)
{
    quad_ctx q = {1.0, 0.0, 0.0}; /* min 0 at 0 */
    double fmin = -1;
    double x = mlab_golden_section(parabola, &q, -1.0, 2.0, 100, 1e-12, &fmin, NULL);
    int ok = fabs(x) < 1e-8 && fabs(fmin) < 1e-14;
    char d[64];
    snprintf(d, sizeof d, "x*=%.3e fmin=%.3e", x, fmin);
    test_record(ok, "linesearch", "golden_zero_minimum", d);
    return ok;
}

/*
 * 路线图 L188 判据：黄金分割实测收缩比与 0.618 相对误差 < 1%。
 * 通过 widths_out 记录每次迭代的区间宽度：w_{k+1}/w_k ≡ φ（每步精确收缩）。
 */
static double g_golden_ratio_emp;

static int t_golden_shrink_ratio_measured(void)
{
    quad_ctx q = {2.0, -2.8, 1.0};
    enum { K = 40 };
    double widths[K];
    int i, cnt = 0, ok;
    double mean_ratio = 0, rel;
    for (i = 0; i < K; ++i) widths[i] = 0.0;
    mlab_golden_section(parabola, &q, 0.0, 4.0, K, 1e-15, NULL, widths);
    while (cnt < K && widths[cnt] > 0.0) ++cnt;
    for (i = 1; i < cnt; ++i) mean_ratio += widths[i] / widths[i - 1];
    mean_ratio /= (cnt > 1) ? (double)(cnt - 1) : 1.0;
    g_golden_ratio_emp = mean_ratio;
    rel = fabs(mean_ratio - PHI) / PHI;
    ok = cnt > 10 && rel < 0.01;
    {
        char d[112];
        snprintf(d, sizeof d, "measured shrink=%.8f vs φ=%.8f rel=%.2e (%d iters)",
                 mean_ratio, PHI, rel, cnt);
        test_record(ok, "linesearch", "golden_shrink_ratio_0618", d);
    }
    return ok;
}

static double f_quad(const double *x, int n, void *ctx)
{
    const double *A = ctx;
    double s = 0;
    int i, j;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) s += 0.5 * x[i] * A[i * n + j] * x[j];
    return s;
}

static void g_quad(const double *x, int n, double *g, void *ctx)
{
    const double *A = ctx;
    int i, j;
    for (i = 0; i < n; ++i) {
        double s = 0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
        g[i] = s;
    }
}

static int make_diag_quad(double *A, int n, double kappa)
{
    int i;
    double *evals = malloc(sizeof(double) * (size_t)n);
    int rc;
    if (!evals) return -1;
    for (i = 0; i < n; ++i)
        evals[i] = 1.0 + (kappa - 1.0) * i / (n > 1 ? n - 1 : 1);
    rc = mlab_spd_from_spectrum(A, n, evals, 5);
    free(evals);
    return rc;
}

/* g_armijo 记录两组 (c1, alpha, fevals) 供 CSV */
static double g_armijo_c1[2], g_armijo_alpha[2];
static int g_armijo_fe[2];

static int armijo_run(double c1, double *alpha_out, int *fe_out)
{
    const int n = 8;
    double A[64], x[8], g[8], d[8];
    mlab_objective obj;
    double alpha;
    int i, fe = 0;
    if (make_diag_quad(A, n, 10) != 0) return 0;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = NULL; obj.dim = n; obj.ctx = A;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    g_quad(x, n, g, A);
    for (i = 0; i < n; ++i) d[i] = -g[i];
    alpha = mlab_armijo_backtrack(&obj, x, d, 1.0, c1, 40, &fe);
    if (alpha_out) *alpha_out = alpha;
    if (fe_out) *fe_out = fe;
    return 1;
}

static int t_armijo_accepts_descent(void)
{
    double alpha = 0;
    int fe = 0;
    int ok = armijo_run(1e-4, &alpha, &fe) && alpha > 0 && fe >= 1;
    g_armijo_c1[0] = 1e-4;
    g_armijo_alpha[0] = alpha;
    g_armijo_fe[0] = fe;
    {
        char di[80];
        snprintf(di, sizeof di, "alpha=%.6f fevals=%d", alpha, fe);
        test_record(ok, "linesearch", "armijo_backtrack_quad", di);
        return ok;
    }
}

static int t_armijo_tighter_c1(void)
{
    double a1 = 0, a2 = 0;
    int fe1 = 0, fe2 = 0;
    int ok = armijo_run(1e-4, &a1, &fe1) && armijo_run(0.4, &a2, &fe2);
    ok = ok && a1 > 0 && a2 > 0 && fe2 >= fe1;
    g_armijo_c1[1] = 0.4;
    g_armijo_alpha[1] = a2;
    g_armijo_fe[1] = fe2;
    {
        char di[96];
        snprintf(di, sizeof di, "c1=1e-4 fe=%d a=%.4f; c1=0.4 fe=%d a=%.4f",
                 fe1, a1, fe2, a2);
        test_record(ok, "linesearch", "armijo_stricter_c1_not_fewer_evals", di);
        return ok;
    }
}

static int t_armijo_rejects_ascent(void)
{
    const int n = 8;
    double A[64], x[8], g[8], d[8];
    mlab_objective obj;
    double alpha;
    int i, fe = 0, ok;
    if (make_diag_quad(A, n, 10) != 0) return 0;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = NULL; obj.dim = n; obj.ctx = A;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    g_quad(x, n, g, A);
    for (i = 0; i < n; ++i) d[i] = g[i]; /* 上升方向 */
    alpha = mlab_armijo_backtrack(&obj, x, d, 1.0, 1e-4, 40, &fe);
    ok = alpha == 0.0 && fe >= 1; /* 必须拒绝并返回 0 */
    {
        char di[80];
        snprintf(di, sizeof di, "ascent dir → alpha=%.1f fevals=%d (expect 0)", alpha, fe);
        test_record(ok, "linesearch", "armijo_rejects_ascent_direction", di);
    }
    return ok;
}

/* ---- strong Wolfe ---- */

static int t_wolfe_quad_accepts_half_exact(void)
{
    const int n = 8;
    double A[64], x[8], g[8], d[8], Ag[8];
    mlab_objective obj;
    double alpha, fe_ = 0, ge_ = 0, a_exact, dphi0, dphi_a, f0, fa;
    int i, fe = 0, ge = 0, ok;
    if (make_diag_quad(A, n, 10) != 0) return 0;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = NULL; obj.dim = n; obj.ctx = A;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    g_quad(x, n, g, A);
    for (i = 0; i < n; ++i) d[i] = -g[i];
    g_quad(d, n, Ag, A); /* A·d（d=-g） */
    a_exact = 0;
    for (i = 0; i < n; ++i) { a_exact += d[i] * d[i]; }
    { double dad = 0; for (i = 0; i < n; ++i) dad += d[i] * Ag[i]; a_exact /= dad; }
    dphi0 = 0;
    for (i = 0; i < n; ++i) dphi0 += g[i] * d[i];
    f0 = f_quad(x, n, A);
    alpha = mlab_wolfe_strong(&obj, x, d, 0.5 * a_exact, 1e-4, 0.9, 50, &fe, &ge);
    (void)fe_; (void)ge_;
    ok = alpha > 0;
    if (ok) {
        double xt[8];
        for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * d[i];
        fa = f_quad(xt, n, A);
        { double gt[8], s = 0; g_quad(xt, n, gt, A); for (i = 0; i < n; ++i) s += gt[i] * d[i]; dphi_a = s; }
        ok = fabs(alpha - 0.5 * a_exact) < 1e-12 &&            /* α0 直接满足两条件 */
             fa <= f0 + 1e-4 * alpha * dphi0 &&                /* Armijo 自洽 */
             fabs(dphi_a) <= 0.9 * fabs(dphi0);                /* 曲率自洽 */
    }
    {
        char di[128];
        snprintf(di, sizeof di, "α=%.6f (0.5αe=%.6f) fe=%d ge=%d 两条件成立",
                 alpha, 0.5 * a_exact, fe, ge);
        test_record(ok, "wolfe", "wolfe_strong_accepts_half_exact", di);
    }
    return ok;
}

static int t_wolfe_quad_zoom_from_overshoot(void)
{
    const int n = 8;
    double A[64], x[8], g[8], d[8], Ag[8];
    mlab_objective obj;
    double alpha, a_exact, dphi0, f0, fa;
    int i, fe = 0, ge = 0, ok;
    if (make_diag_quad(A, n, 10) != 0) return 0;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = NULL; obj.dim = n; obj.ctx = A;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    g_quad(x, n, g, A);
    for (i = 0; i < n; ++i) d[i] = -g[i];
    g_quad(d, n, Ag, A);
    a_exact = 0;
    for (i = 0; i < n; ++i) a_exact += d[i] * d[i];
    { double dad = 0; for (i = 0; i < n; ++i) dad += d[i] * Ag[i]; a_exact /= dad; }
    dphi0 = 0;
    for (i = 0; i < n; ++i) dphi0 += g[i] * d[i];
    f0 = f_quad(x, n, A);
    /* α0=3αe 越过极小（Armijo 失败）→ 触发 zoom */
    alpha = mlab_wolfe_strong(&obj, x, d, 3.0 * a_exact, 1e-4, 0.9, 50, &fe, &ge);
    ok = alpha > 0 && alpha <= 3.0 * a_exact;
    if (ok) {
        double xt[8];
        for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * d[i];
        fa = f_quad(xt, n, A);
        { double gt[8], s = 0; g_quad(xt, n, gt, A); for (i = 0; i < n; ++i) s += gt[i] * d[i]; ok = fabs(s) <= 0.9 * fabs(dphi0); }
        ok = ok && fa < f0; /* zoom 结果必须下降且满足曲率 */
    }
    {
        char di[128];
        snprintf(di, sizeof di, "α0=3αe=%.4f → zoom α=%.6f (αe=%.4f), f 下降, fe=%d",
                 3.0 * a_exact, alpha, a_exact, fe);
        test_record(ok, "wolfe", "wolfe_strong_zoom_from_overshoot", di);
    }
    return ok;
}

static int t_wolfe_rejects_ascent(void)
{
    const int n = 8;
    double A[64], x[8], g[8], d[8];
    mlab_objective obj;
    double alpha;
    int i, fe = 0, ge = 0, ok;
    if (make_diag_quad(A, n, 10) != 0) return 0;
    obj.f = f_quad; obj.grad = g_quad; obj.hess = NULL; obj.dim = n; obj.ctx = A;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    g_quad(x, n, g, A);
    for (i = 0; i < n; ++i) d[i] = g[i]; /* 上升方向 */
    alpha = mlab_wolfe_strong(&obj, x, d, 1.0, 1e-4, 0.9, 50, &fe, &ge);
    ok = alpha == 0.0 && fe >= 1;
    {
        char di[80];
        snprintf(di, sizeof di, "ascent dir → alpha=%.1f (expect 0)", alpha);
        test_record(ok, "wolfe", "wolfe_rejects_ascent_direction", di);
    }
    return ok;
}

/* ---- steepest descent ---- */

static int run_sd_iters(double kappa, int *iters_out, double *res_hist, int res_cap)
{
    const int n = 20;
    double A[400], x[20];
    int it = 0, fe = 0, i;
    if (make_diag_quad(A, n, kappa) != 0) return 0;
    for (i = 0; i < n; ++i) x[i] = 1.0;
    if (mlab_steepest_descent_quad(A, n, x, 8000, 1e-10, res_hist, &it, &fe) != 0)
        return 0;
    *iters_out = it;
    (void)res_cap;
    return 1;
}

static int t_sd_well_vs_ill(void)
{
    int iw = 0, ii = 0;
    double ratio, theory = 200.0 / 5.0;
    int ok = run_sd_iters(5.0, &iw, NULL, 0) && run_sd_iters(200.0, &ii, NULL, 0);
    ratio = ok ? (double)ii / (double)iw : 0;
    /* 路线图 L189：病态/良态迭代数之比与条件数理论预测同数量级 */
    ok = ok && ratio > 0.2 * theory && ratio < 5.0 * theory && ii > iw;
    {
        char d[96];
        snprintf(d, sizeof d, "well(κ=5)=%d ill(κ=200)=%d ratio=%.1f theory~%.0f",
                 iw, ii, ratio, theory);
        test_record(ok, "steepest_descent", "ill_conditioned_more_iters", d);
        return ok;
    }
}

static int t_sd_quadratic_converges(void)
{
    int it = 0;
    int ok = run_sd_iters(2.0, &it, NULL, 0) && it > 0 && it < 5000;
    char d[48];
    snprintf(d, sizeof d, "κ=2 iters=%d", it);
    test_record(ok, "steepest_descent", "well_conditioned_converges", d);
    return ok;
}

/* ---- results CSV（全量运行时重算落盘） ---- */

static void write_csvs(void)
{
    FILE *fp;
    /* golden.csv：区间宽度与每步收缩比 */
    fp = fopen("results/golden.csv", "w");
    if (fp) {
        quad_ctx q = {2.0, -2.8, 1.0};
        enum { K = 40 };
        double widths[K];
        int i, cnt = 0;
        for (i = 0; i < K; ++i) widths[i] = 0.0;
        mlab_golden_section(parabola, &q, 0.0, 4.0, K, 1e-15, NULL, widths);
        while (cnt < K && widths[cnt] > 0.0) ++cnt;
        fprintf(fp, "iter,width,shrink_ratio\n");
        fprintf(fp, "0,%.15e,\n", 4.0);
        for (i = 1; i < cnt; ++i)
            fprintf(fp, "%d,%.15e,%.15f\n", i, widths[i], widths[i] / widths[i - 1]);
        fclose(fp);
    }
    /* armijo.csv：c1 扫描 */
    fp = fopen("results/armijo.csv", "w");
    if (fp) {
        fprintf(fp, "c1,alpha,fevals\n");
        fprintf(fp, "0.0001,%.8f,%d\n", g_armijo_alpha[0], g_armijo_fe[0]);
        fprintf(fp, "0.4,%.8f,%d\n", g_armijo_alpha[1], g_armijo_fe[1]);
        fclose(fp);
    }
    /* gd_traj.csv：病态/良态迭代轨迹（锯齿可见） */
    fp = fopen("results/gd_traj.csv", "w");
    if (fp) {
        static const double kap[2] = {5.0, 200.0};
        enum { CAP = 8000 };
        int k;
        fprintf(fp, "kappa,iter,residual\n");
        for (k = 0; k < 2; ++k) {
            double *res = malloc(sizeof(double) * CAP);
            int it = 0, i;
            if (!res) break;
            if (run_sd_iters(kap[k], &it, res, CAP))
                for (i = 0; i < it; ++i)
                    fprintf(fp, "%.0f,%d,%.6e\n", kap[k], i, res[i]);
            free(res);
        }
        fclose(fp);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"linesearch", "golden_parabola_minimum", "黄金分割求抛物线最小", t_golden_parabola_min},
        {"linesearch", "golden_zero_minimum", "黄金分割夹逼到 0", t_golden_zero_min},
        {"linesearch", "golden_shrink_ratio_0618", "收缩比实测 ≈0.618（<1%，路线图 L188）", t_golden_shrink_ratio_measured},
        {"linesearch", "armijo_backtrack_quad", "Armijo 在二次函数上接受下降步", t_armijo_accepts_descent},
        {"linesearch", "armijo_stricter_c1_not_fewer_evals", "更严 c1 不会更少回退评估", t_armijo_tighter_c1},
        {"linesearch", "armijo_rejects_ascent_direction", "Armijo 拒绝非下降方向", t_armijo_rejects_ascent},
        {"wolfe", "wolfe_strong_accepts_half_exact", "强 Wolfe 在 0.5αe 直接接受且条件自洽", t_wolfe_quad_accepts_half_exact},
        {"wolfe", "wolfe_strong_zoom_from_overshoot", "α0=3αe 触发 zoom 并满足条件", t_wolfe_quad_zoom_from_overshoot},
        {"wolfe", "wolfe_rejects_ascent_direction", "强 Wolfe 拒绝上升方向", t_wolfe_rejects_ascent},
        {"steepest_descent", "well_conditioned_converges", "良态二次 GD 收敛", t_sd_quadratic_converges},
        {"steepest_descent", "ill_conditioned_more_iters", "病态迭代数显著更多（判据 L189）", t_sd_well_vs_ill},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("B1-line-search-gd", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
