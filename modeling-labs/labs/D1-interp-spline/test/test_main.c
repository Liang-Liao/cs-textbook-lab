/*
 * D1: 插值与样条 — 多项式 / Runge / 样条收敛阶 / RBF 形状参数
 */
#include "harness.h"
#include "lab.h"
#include "interp.h"
#include "linalg.h"
#include "rng.h"
#include "gp.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double runge(double x)
{
    return 1.0 / (1.0 + 25.0 * x * x);
}

static double smooth1d(double x)
{
    /* f''(0)=f''(1)=0，适合自然边界三次样条测 O(h^4) */
    return sin(M_PI * x);
}

static double truth2d_smooth(const double *p)
{
    double x = p[0], y = p[1];
    return exp(-(x * x + y * y)) + 0.35 * sin(2.5 * x) * cos(1.8 * y);
}

static double max_abs_diff_eval(
    double (*interp)(void *ctx, double x), void *ctx,
    double (*f)(double), double a, double b, int ngrid)
{
    double m = 0.0;
    int i;
    for (i = 0; i <= ngrid; ++i) {
        double x = a + (b - a) * (double)i / (double)ngrid;
        double e = fabs(interp(ctx, x) - f(x));
        if (e > m) m = e;
    }
    return m;
}

static double wrap_newton(void *ctx, double x)
{
    double **pp = (double **)ctx;
    return mlab_newton_eval(pp[0], pp[1], *(int *)pp[2], x);
}

static double wrap_pwl(void *ctx, double x)
{
    double **pp = (double **)ctx;
    return mlab_piecewise_linear_eval(pp[0], pp[1], *(int *)pp[2], x);
}

static double wrap_cspline(void *ctx, double x)
{
    return mlab_cspline_eval((const mlab_cspline *)ctx, x);
}

/* log-log 最小二乘斜率 */
static double loglog_slope(const double *h, const double *err, int n)
{
    double sx = 0, sy = 0, sxx = 0, sxy = 0, den, m;
    int i;
    if (n < 2) return 0.0;
    for (i = 0; i < n; ++i) {
        double lx = log(h[i]), ly = log(err[i]);
        sx += lx;
        sy += ly;
        sxx += lx * lx;
        sxy += lx * ly;
    }
    den = (double)n * sxx - sx * sx;
    if (fabs(den) < 1e-300) return 0.0;
    m = ((double)n * sxy - sx * sy) / den;
    return m;
}

/* ---------- suite: poly ---------- */

static int t_poly_lagrange_exact_on_low_degree(void)
{
    /* 三次多项式用 4 节点 Lagrange/Newton 应精确 */
    const int n = 4;
    double xn[4] = {-1.0, -0.25, 0.4, 1.2};
    double yn[4];
    double coef[4];
    double max_l = 0.0, max_n = 0.0;
    int i, ok;
    char detail[180];

    for (i = 0; i < n; ++i) {
        double x = xn[i];
        /* p(x) = 2x^3 - x^2 + 0.5x - 1 */
        yn[i] = ((2.0 * x - 1.0) * x + 0.5) * x - 1.0;
    }
    if (mlab_newton_divided_diff(xn, yn, n, coef) != 0) {
        test_record(0, "poly", "lagrange_exact_on_poly_deg_le_n-1", "dd fail");
        return 0;
    }
    for (i = 0; i <= 50; ++i) {
        double x = -1.2 + 2.6 * (double)i / 50.0;
        double true_v = ((2.0 * x - 1.0) * x + 0.5) * x - 1.0;
        double el = fabs(mlab_lagrange_eval(xn, yn, n, x) - true_v);
        double en = fabs(mlab_newton_eval(xn, coef, n, x) - true_v);
        if (el > max_l) max_l = el;
        if (en > max_n) max_n = en;
    }
    ok = (max_l < 1e-12) && (max_n < 1e-12);
    snprintf(detail, sizeof detail,
             "deg=3 n=4 max|L-p|=%.3e max|N-p|=%.3e (<1e-12)", max_l, max_n);
    test_record(ok, "poly", "lagrange_exact_on_poly_deg_le_n-1", detail);
    return ok;
}

static int t_poly_newton_matches_lagrange(void)
{
    const int n = 7;
    double xn[7], yn[7], coef[7];
    double maxdiff = 0.0;
    mlab_rng rng;
    int i, ok;
    char detail[160];

    mlab_rng_seed(&rng, 71001);
    for (i = 0; i < n; ++i) {
        xn[i] = -1.0 + 2.0 * (double)i / (double)(n - 1);
        xn[i] += 0.01 * (mlab_rng_uniform(&rng) - 0.5);
        yn[i] = runge(xn[i]) + 0.05 * sin(7.0 * xn[i]);
    }
    if (mlab_newton_divided_diff(xn, yn, n, coef) != 0) {
        test_record(0, "poly", "newton_matches_lagrange", "dd fail");
        return 0;
    }
    for (i = 0; i <= 40; ++i) {
        double x = -1.05 + 2.1 * (double)i / 40.0;
        double a = mlab_lagrange_eval(xn, yn, n, x);
        double b = mlab_newton_eval(xn, coef, n, x);
        double d = fabs(a - b);
        if (d > maxdiff) maxdiff = d;
    }
    ok = maxdiff < 1e-9;
    snprintf(detail, sizeof detail, "max|L-N|=%.3e on random nodes n=7", maxdiff);
    test_record(ok, "poly", "newton_matches_lagrange", detail);
    return ok;
}

/* ---------- suite: runge ---------- */

static int t_runge_high_degree_blowup(void)
{
    /*
     * Runge 现象：高次等距节点多项式在端点附近误差爆炸。
     * 判据锚点：n=21 时 max|err| 显著大于分段线性/样条。
     */
    const int n = 21;
    double xn[21], yn[21], coef[21];
    mlab_cspline sp;
    double max_poly = 0.0, max_pwl = 0.0, max_cs = 0.0;
    int i, ok;
    char detail[220];
    void *ctx_poly[3];
    void *ctx_pwl[3];
    int n_i = n;

    for (i = 0; i < n; ++i) {
        xn[i] = -1.0 + 2.0 * (double)i / (double)(n - 1);
        yn[i] = runge(xn[i]);
    }
    if (mlab_newton_divided_diff(xn, yn, n, coef) != 0 ||
        mlab_cspline_fit(&sp, xn, yn, n) != 0) {
        test_record(0, "runge", "high_degree_blowup_vs_piecewise", "fit fail");
        return 0;
    }
    ctx_poly[0] = xn;
    ctx_poly[1] = coef;
    ctx_poly[2] = &n_i;
    ctx_pwl[0] = xn;
    ctx_pwl[1] = yn;
    ctx_pwl[2] = &n_i;
    max_poly = max_abs_diff_eval(wrap_newton, ctx_poly, runge, -1.0, 1.0, 400);
    max_pwl = max_abs_diff_eval(wrap_pwl, ctx_pwl, runge, -1.0, 1.0, 400);
    max_cs = max_abs_diff_eval(wrap_cspline, &sp, runge, -1.0, 1.0, 400);

    /* 多项式误差应显著更大（≥5 倍）且绝对值不可忽略 */
    ok = (max_poly > 0.3) && (max_poly >= 5.0 * max_cs) &&
         (max_poly >= 5.0 * max_pwl);
    snprintf(detail, sizeof detail,
             "n=21 Runge maxerr poly=%.4g pwl=%.4g cspline=%.4g",
             max_poly, max_pwl, max_cs);
    test_record(ok, "runge", "high_degree_blowup_vs_piecewise", detail);
    mlab_cspline_free(&sp);
    return ok;
}

static int t_runge_spline_better_than_high_poly(void)
{
    /* 对照：同一节点数下自然三次样条优于高次 Newton */
    const int n = 15;
    double xn[15], yn[15], coef[15];
    mlab_cspline sp;
    double max_poly, max_cs;
    int i, ok;
    char detail[180];
    void *ctx_poly[3];
    int n_i = n;

    for (i = 0; i < n; ++i) {
        xn[i] = -1.0 + 2.0 * (double)i / (double)(n - 1);
        yn[i] = runge(xn[i]);
    }
    if (mlab_newton_divided_diff(xn, yn, n, coef) != 0 ||
        mlab_cspline_fit(&sp, xn, yn, n) != 0) {
        test_record(0, "runge", "spline_better_than_high_degree", "fit fail");
        return 0;
    }
    ctx_poly[0] = xn;
    ctx_poly[1] = coef;
    ctx_poly[2] = &n_i;
    max_poly = max_abs_diff_eval(wrap_newton, ctx_poly, runge, -1.0, 1.0, 400);
    max_cs = max_abs_diff_eval(wrap_cspline, &sp, runge, -1.0, 1.0, 400);
    ok = (max_cs < 0.25 * max_poly) && (max_cs < 0.15);
    snprintf(detail, sizeof detail,
             "n=15 maxerr spline=%.4g < 0.25*poly=%.4g, spline<0.15",
             max_cs, max_poly);
    test_record(ok, "runge", "spline_better_than_high_degree", detail);
    mlab_cspline_free(&sp);
    return ok;
}

/* ---------- suite: spline ---------- */

static int t_spline_interpolates_nodes_and_bc(void)
{
    const int n = 12;
    double xn[12], yn[12];
    mlab_cspline sp;
    double max_node = 0.0;
    int i, ok;
    char detail[160];

    for (i = 0; i < n; ++i) {
        xn[i] = (double)i / (double)(n - 1);
        yn[i] = smooth1d(xn[i]);
    }
    if (mlab_cspline_fit(&sp, xn, yn, n) != 0) {
        test_record(0, "spline", "natural_bc_and_node_exact", "fit fail");
        return 0;
    }
    for (i = 0; i < n; ++i) {
        double e = fabs(mlab_cspline_eval(&sp, xn[i]) - yn[i]);
        if (e > max_node) max_node = e;
    }
    /* 自然边界：M0=M_{n-1}=0；节点插值精确 */
    ok = (max_node < 1e-12) && (fabs(sp.M[0]) < 1e-14) &&
         (fabs(sp.M[n - 1]) < 1e-14);
    snprintf(detail, sizeof detail,
             "max|s(x_i)-y_i|=%.3e; M0=%.2e M_end=%.2e", max_node, sp.M[0], sp.M[n - 1]);
    test_record(ok, "spline", "natural_bc_and_node_exact", detail);
    mlab_cspline_free(&sp);
    return ok;
}

static int t_spline_convergence_order_4(void)
{
    /*
     * 判据：三次样条实测收敛阶 4±0.3。
     * f(x)=sin(πx) on [0,1]，端点二阶导为 0，自然边界不引入 O(h^2) 主导误差。
     */
    const int ns = 5;
    const int node_counts[5] = {9, 17, 33, 65, 129};
    double hs[5], errs[5];
    double order;
    int k, ok;
    char detail[200];
    FILE *fp;

    test_ensure_results_dir();
    fp = fopen("results/spline_convergence.csv", "w");
    if (fp) fprintf(fp, "n,h,max_err\n");
    for (k = 0; k < ns; ++k) {
        int n = node_counts[k];
        double *xn = (double *)malloc((size_t)n * sizeof(double));
        double *yn = (double *)malloc((size_t)n * sizeof(double));
        mlab_cspline sp;
        int i;
        double maxe = 0.0;
        if (!xn || !yn) {
            free(xn);
            free(yn);
            test_record(0, "spline", "convergence_order_4", "oom");
            return 0;
        }
        for (i = 0; i < n; ++i) {
            xn[i] = (double)i / (double)(n - 1);
            yn[i] = smooth1d(xn[i]);
        }
        if (mlab_cspline_fit(&sp, xn, yn, n) != 0) {
            free(xn);
            free(yn);
            test_record(0, "spline", "convergence_order_4", "fit fail");
            return 0;
        }
        /* 子区间中点上测误差（避开节点） */
        for (i = 0; i < n - 1; ++i) {
            double mid = 0.5 * (xn[i] + xn[i + 1]);
            double e = fabs(mlab_cspline_eval(&sp, mid) - smooth1d(mid));
            if (e > maxe) maxe = e;
        }
        hs[k] = 1.0 / (double)(n - 1);
        errs[k] = maxe > 0 ? maxe : 1e-300;
        if (fp) fprintf(fp, "%d,%.8g,%.8g\n", n, hs[k], errs[k]);
        mlab_cspline_free(&sp);
        free(xn);
        free(yn);
    }
    if (fp) fclose(fp);
    order = fabs(loglog_slope(hs, errs, ns));
    ok = (order >= 4.0 - 0.3) && (order <= 4.0 + 0.3);
    snprintf(detail, sizeof detail,
             "log-log |slope|=%.3f (target 4±0.3); err(1/8)=%.3e err(1/128)=%.3e",
             order, errs[0], errs[ns - 1]);
    test_record(ok, "spline", "convergence_order_4", detail);
    return ok;
}

static int t_spline_thomas_vs_a1_lu(void)
{
    /* 三对角 Thomas 与 A1 LU 在同一系统上解一致 */
    const int n = 8;
    double a[8], b[8], c[8], d[8], x_th[8], x_lu[8];
    double A[64];
    int i, ok;
    double maxdiff = 0.0;
    char detail[160];
    mlab_rng rng;

    mlab_rng_seed(&rng, 72002);
    for (i = 0; i < n; ++i) {
        a[i] = (i == 0) ? 0.0 : 0.4 + 0.1 * mlab_rng_uniform(&rng);
        b[i] = 2.5 + mlab_rng_uniform(&rng);
        c[i] = (i == n - 1) ? 0.0 : 0.4 + 0.1 * mlab_rng_uniform(&rng);
        d[i] = mlab_rng_uniform(&rng) - 0.5;
    }
    memset(A, 0, sizeof A);
    for (i = 0; i < n; ++i) {
        if (i > 0) A[i * n + (i - 1)] = a[i];
        A[i * n + i] = b[i];
        if (i < n - 1) A[i * n + (i + 1)] = c[i];
    }
    if (mlab_thomas(n, a, b, c, d, x_th) != 0 ||
        mlab_lu_solve_dense(A, n, d, x_lu) != 0) {
        test_record(0, "spline", "thomas_matches_a1_lu", "solve fail");
        return 0;
    }
    for (i = 0; i < n; ++i) {
        double diff = fabs(x_th[i] - x_lu[i]);
        if (diff > maxdiff) maxdiff = diff;
    }
    ok = maxdiff < 1e-10;
    snprintf(detail, sizeof detail, "max|Thomas-LU|=%.3e (n=%d)", maxdiff, n);
    test_record(ok, "spline", "thomas_matches_a1_lu", detail);
    return ok;
}

/* ---------- suite: hermite ---------- */

static int t_hermite_matches_value_and_derivative(void)
{
    const int n = 10;
    double xn[10], yn[10], yp[10];
    double max_v = 0.0;
    int i, ok;
    char detail[160];

    for (i = 0; i < n; ++i) {
        xn[i] = 0.0 + 1.5 * (double)i / (double)(n - 1);
        yn[i] = sin(xn[i]);
        yp[i] = cos(xn[i]); /* 真导 */
    }
    /* 节点处值精确 */
    for (i = 0; i < n; ++i) {
        double e = fabs(mlab_cubic_hermite_eval(xn, yn, yp, n, xn[i]) - yn[i]);
        if (e > max_v) max_v = e;
    }
    /* 一阶导：中心差分对比 hermite 的局部导近似 */
    {
        double max_d = 0.0;
        for (i = 1; i < n - 1; ++i) {
            double h = 1e-6;
            double x = xn[i];
            double sp = (mlab_cubic_hermite_eval(xn, yn, yp, n, x + h) -
                         mlab_cubic_hermite_eval(xn, yn, yp, n, x - h)) / (2.0 * h);
            double e = fabs(sp - yp[i]);
            if (e > max_d) max_d = e;
        }
        ok = (max_v < 1e-14) && (max_d < 1e-4);
        snprintf(detail, sizeof detail,
                 "node value err=%.3e; deriv FD vs yp max=%.3e", max_v, max_d);
    }
    test_record(ok, "hermite", "node_value_and_slope_match", detail);
    return ok;
}

static int t_hermite_smooth_join(void)
{
    /* 相邻段在公共节点 C1：左右极限值/导连续（由构造保证，数值验证） */
    const int n = 6;
    double xn[6], yn[6], yp[6];
    double max_jump = 0.0;
    int i, ok;
    char detail[160];

    for (i = 0; i < n; ++i) {
        xn[i] = -0.5 + 1.2 * (double)i / (double)(n - 1);
        yn[i] = exp(-xn[i] * xn[i]);
        yp[i] = -2.0 * xn[i] * yn[i];
    }
    for (i = 1; i < n - 1; ++i) {
        double h = 1e-8;
        double xl = mlab_cubic_hermite_eval(xn, yn, yp, n, xn[i] - h);
        double xr = mlab_cubic_hermite_eval(xn, yn, yp, n, xn[i] + h);
        double y0 = yn[i];
        double jump = fmax(fabs(xl - y0), fabs(xr - y0));
        if (jump > max_jump) max_jump = jump;
    }
    ok = max_jump < 1e-6;
    snprintf(detail, sizeof detail, "max |s(x_i±h)-y_i|=%.3e (<1e-6)", max_jump);
    test_record(ok, "hermite", "c1_join_continuous", detail);
    return ok;
}

/* ---------- suite: rbf ---------- */

typedef struct {
    double eps_log_min, eps_log_max;
    int n_eps;
    int n_pts;
    unsigned seed;
} rbf_scan_cfg;

static int t_rbf_gaussian_shape_optimal_region(void)
{
    /*
     * 2D 散点 RBF（高斯核）形状参数扫描：
     * - 最优区域：节点插值残差 < 1e-6，核矩阵条件数可控
     * - 区域外：小 ε 病态化（cond 暴涨 / 残差恶化）可复现
     */
    rbf_scan_cfg cfg;
    double *X = NULL, *y = NULL;
    double *eps_list = NULL, *resid = NULL, *grid_err = NULL, *cond_list = NULL;
    double *pgm_img = NULL;
    mlab_rng rng;
    int i, e, ok;
    int n_good = 0;
    double eps_opt = 0.0, cond_opt = 0.0, res_opt = 0.0, gerr_opt = 1e300;
    double res_small = 0.0, cond_small = 0.0;
    int has_small = 0;
    char detail[260];
    FILE *fp;

    cfg.eps_log_min = log(0.15);
    cfg.eps_log_max = log(25.0);
    cfg.n_eps = 36;
    cfg.n_pts = 36;
    cfg.seed = 73003;

    X = (double *)malloc((size_t)cfg.n_pts * 2 * sizeof(double));
    y = (double *)malloc((size_t)cfg.n_pts * sizeof(double));
    eps_list = (double *)malloc((size_t)cfg.n_eps * sizeof(double));
    resid = (double *)malloc((size_t)cfg.n_eps * sizeof(double));
    grid_err = (double *)malloc((size_t)cfg.n_eps * sizeof(double));
    cond_list = (double *)malloc((size_t)cfg.n_eps * sizeof(double));
    pgm_img = (double *)malloc((size_t)2 * (size_t)cfg.n_eps * sizeof(double));
    if (!X || !y || !eps_list || !resid || !grid_err || !cond_list || !pgm_img) {
        free(X); free(y); free(eps_list); free(resid);
        free(grid_err); free(cond_list); free(pgm_img);
        test_record(0, "rbf", "gaussian_shape_optimal_region", "oom");
        return 0;
    }

    mlab_rng_seed(&rng, cfg.seed);
    /* 散点：抖动网格，保证覆盖 [-1,1]^2 */
    {
        int side = 6, k = 0;
        int a, b;
        for (a = 0; a < side && k < cfg.n_pts; ++a) {
            for (b = 0; b < side && k < cfg.n_pts; ++b) {
                X[k * 2 + 0] = -0.9 + 1.8 * ((double)a + 0.15 + 0.7 * mlab_rng_uniform(&rng)) / (double)side;
                X[k * 2 + 1] = -0.9 + 1.8 * ((double)b + 0.15 + 0.7 * mlab_rng_uniform(&rng)) / (double)side;
                y[k] = truth2d_smooth(&X[k * 2]);
                ++k;
            }
        }
        cfg.n_pts = k;
    }

    test_ensure_results_dir();
    fp = fopen("results/rbf_shape_scan.csv", "w");
    if (fp) fprintf(fp, "eps,node_resid,grid_err,cond2\n");

    for (e = 0; e < cfg.n_eps; ++e) {
        double t = (double)e / (double)(cfg.n_eps - 1);
        double eps = exp(cfg.eps_log_min + t * (cfg.eps_log_max - cfg.eps_log_min));
        mlab_rbf rbf;
        double max_res = 0.0, max_ge = 0.0, cond;
        int gi;

        eps_list[e] = eps;
        if (mlab_rbf_fit(&rbf, X, y, cfg.n_pts, 2, MLAB_RBF_GAUSSIAN, eps) != 0) {
            resid[e] = 1e300;
            grid_err[e] = 1e300;
            cond_list[e] = 1e300;
            if (fp) fprintf(fp, "%.6g,%.6g,%.6g,%.6g\n", eps, 1e300, 1e300, 1e300);
            continue;
        }
        for (i = 0; i < cfg.n_pts; ++i) {
            double s = mlab_rbf_eval(&rbf, &X[i * 2]);
            double r = fabs(s - y[i]);
            if (r > max_res) max_res = r;
        }
        for (gi = 0; gi <= 20; ++gi) {
            int gj;
            for (gj = 0; gj <= 20; ++gj) {
                double p[2];
                double s, tge;
                p[0] = -0.95 + 1.9 * (double)gi / 20.0;
                p[1] = -0.95 + 1.9 * (double)gj / 20.0;
                s = mlab_rbf_eval(&rbf, p);
                tge = fabs(s - truth2d_smooth(p));
                if (tge > max_ge) max_ge = tge;
            }
        }
        cond = mlab_rbf_cond2(&rbf);
        resid[e] = max_res;
        grid_err[e] = max_ge;
        cond_list[e] = cond > 0 ? cond : 1e300;
        if (fp)
            fprintf(fp, "%.6g,%.6g,%.6g,%.6g\n",
                    eps, max_res, max_ge, cond_list[e]);
        /* log10 映射进 PGM 亮度（两行：网格误差 / 条件数） */
        pgm_img[e] = log10(max_ge > 1e-16 ? max_ge : 1e-16);
        pgm_img[cfg.n_eps + e] = log10(cond_list[e] > 1 ? cond_list[e] : 1.0);
        mlab_rbf_free(&rbf);
    }
    if (fp) fclose(fp);
    mlab_write_pgm_grid("results/rbf_shape_scan.pgm", pgm_img, cfg.n_eps, 2);

    /* 最优区域：节点残差 < 1e-6 且 cond < 1e10；记录网格误差最小者 */
    for (e = 0; e < cfg.n_eps; ++e) {
        if (resid[e] < 1e-6 && cond_list[e] < 1e10) {
            ++n_good;
            if (grid_err[e] < gerr_opt) {
                gerr_opt = grid_err[e];
                res_opt = resid[e];
                cond_opt = cond_list[e];
                eps_opt = eps_list[e];
            }
        }
        if (eps_list[e] < 0.4 && !has_small) {
            res_small = resid[e];
            cond_small = cond_list[e];
            has_small = 1;
        }
    }

    /*
     * 区域外病态化：取最小 ε 处 cond 显著大于最优区，
     * 或小 ε 时节点残差已 > 1e-3（求解失真）。
     */
    ok = (n_good >= 3) && (res_opt < 1e-6) &&
         (has_small) &&
         ((cond_small > 1e3 * fmax(cond_opt, 1.0)) || (res_small > 1e-3));
    snprintf(detail, sizeof detail,
             "opt ε=%.3g node_resid=%.3e grid_err=%.3e cond=%.3e; "
             "smallε resid=%.3e cond=%.3e; n_good=%d/%d",
             eps_opt, res_opt, gerr_opt, cond_opt, res_small, cond_small,
             n_good, cfg.n_eps);
    test_record(ok, "rbf", "gaussian_shape_optimal_region", detail);

    free(X); free(y); free(eps_list); free(resid);
    free(grid_err); free(cond_list); free(pgm_img);
    return ok;
}

static int t_rbf_tps_interpolates_nodes(void)
{
    /* 薄板样条核（含 degree-1 多项式增广）在光滑函数节点上插值残差应很小 */
    const int n_pts = 25;
    double X[50], y[25];
    mlab_rbf rbf;
    mlab_rng rng;
    int i, side = 5, a, b, k = 0, ok;
    double max_res = 0.0;
    char detail[160];

    mlab_rng_seed(&rng, 73004);
    for (a = 0; a < side; ++a) {
        for (b = 0; b < side; ++b) {
            X[k * 2 + 0] = -0.8 + 1.6 * ((double)a + 0.2 + 0.6 * mlab_rng_uniform(&rng)) / (double)side;
            X[k * 2 + 1] = -0.8 + 1.6 * ((double)b + 0.2 + 0.6 * mlab_rng_uniform(&rng)) / (double)side;
            y[k] = truth2d_smooth(&X[k * 2]);
            ++k;
        }
    }
    if (mlab_rbf_fit(&rbf, X, y, n_pts, 2, MLAB_RBF_TPS, 0.0) != 0) {
        test_record(0, "rbf", "tps_interpolates_nodes", "fit fail");
        return 0;
    }
    for (i = 0; i < n_pts; ++i) {
        double r = fabs(mlab_rbf_eval(&rbf, &X[i * 2]) - y[i]);
        if (r > max_res) max_res = r;
    }
    ok = max_res < 1e-6;
    snprintf(detail, sizeof detail,
             "TPS(+poly aug) max node residual=%.3e (n=%d)", max_res, n_pts);
    test_record(ok, "rbf", "tps_interpolates_nodes", detail);
    mlab_rbf_free(&rbf);
    return ok;
}

static int t_rbf_tps_reproduces_affine(void)
{
    /*
     * TPS 多项式增广的再生性：degree-1 增广使插值体精确再生
     * 任意一次多项式（节点数据来自仿射函数 → 密集点上残差 ~ 机器精度）。
     */
    const int side = 6, n_pts = side * side;
    double X[72], y[36];
    mlab_rbf rbf;
    mlab_rng rng;
    int i, a, b, k = 0, ok;
    double max_res = 0.0;
    char detail[160];

    mlab_rng_seed(&rng, 73005);
    for (a = 0; a < side; ++a) {
        for (b = 0; b < side; ++b) {
            X[k * 2 + 0] = -0.75 + 1.5 * ((double)a + 0.25 * mlab_rng_uniform(&rng)) / (double)side;
            X[k * 2 + 1] = -0.75 + 1.5 * ((double)b + 0.25 * mlab_rng_uniform(&rng)) / (double)side;
            y[k] = 3.0 + 1.5 * X[k * 2] - 2.2 * X[k * 2 + 1];
            ++k;
        }
    }
    if (mlab_rbf_fit(&rbf, X, y, n_pts, 2, MLAB_RBF_TPS, 0.0) != 0) {
        test_record(0, "rbf", "tps_reproduces_affine", "fit fail");
        return 0;
    }
    /* 密集验证点（含非节点坐标） */
    for (i = 0; i < 400; ++i) {
        double p[2];
        double true_v, s;
        p[0] = -0.8 + 1.6 * mlab_rng_uniform(&rng);
        p[1] = -0.8 + 1.6 * mlab_rng_uniform(&rng);
        true_v = 3.0 + 1.5 * p[0] - 2.2 * p[1];
        s = mlab_rbf_eval(&rbf, p);
        if (fabs(s - true_v) > max_res) max_res = fabs(s - true_v);
    }
    ok = max_res < 1e-9;
    snprintf(detail, sizeof detail,
             "TPS vs affine max residual=%.3e (n=%d, 400 dense pts)", max_res, n_pts);
    test_record(ok, "rbf", "tps_reproduces_affine", detail);
    mlab_rbf_free(&rbf);
    return ok;
}

static int t_rbf_duplicate_nodes_rejected(void)
{
    /* 重复节点：fit 应报错（-2）而非给奇异系统的解 */
    double X[8] = {0.0, 0.0,  0.5, 0.1,  0.0, 0.0,  0.9, 0.9};
    double y[4] = {1.0, 2.0, 3.0, 4.0};
    mlab_rbf rbf;
    int rc_tps, rc_gauss;
    char detail[160];

    rc_tps = mlab_rbf_fit(&rbf, X, y, 4, 2, MLAB_RBF_TPS, 0.0);
    if (rc_tps == 0) mlab_rbf_free(&rbf);
    rc_gauss = mlab_rbf_fit(&rbf, X, y, 4, 2, MLAB_RBF_GAUSSIAN, 1.0);
    if (rc_gauss == 0) mlab_rbf_free(&rbf);
    snprintf(detail, sizeof detail,
             "duplicate node rejected: TPS rc=%d Gaussian rc=%d (expect -2)",
             rc_tps, rc_gauss);
    test_record(rc_tps == -2 && rc_gauss == -2, "rbf",
                "duplicate_nodes_rejected", detail);
    return rc_tps == -2 && rc_gauss == -2;
}

static int t_rbf_dense_region_residual(void)
{
    /*
     * 路线图 :608 判据的加密散点验证：最优形状参数区域内
     * 「区域内插值残差 < 1e-6」用密集随机验证点检验（非仅节点）。
     * 节点 30×30 抖动网格（h≈0.062），ε=1.0；3000 个 [−0.95,0.95]² 随机点。
     */
    const int side = 30, n_pts = side * side, n_dense = 3000;
    double *X = (double *)malloc((size_t)n_pts * 2 * sizeof(double));
    double *y = (double *)malloc((size_t)n_pts * sizeof(double));
    mlab_rbf rbf;
    mlab_rng rng;
    int a, b, k = 0, i, ok;
    double max_res = 0.0;
    char detail[180];
    FILE *fp;

    if (!X || !y) {
        free(X); free(y);
        test_record(0, "rbf", "dense_region_residual_1e-6", "oom");
        return 0;
    }
    mlab_rng_seed(&rng, 73006);
    for (a = 0; a < side; ++a) {
        for (b = 0; b < side; ++b) {
            X[k * 2 + 0] = -0.9 + 1.8 * ((double)a + mlab_rng_uniform(&rng)) / (double)(side - 1);
            X[k * 2 + 1] = -0.9 + 1.8 * ((double)b + mlab_rng_uniform(&rng)) / (double)(side - 1);
            y[k] = truth2d_smooth(&X[k * 2]);
            ++k;
        }
    }
    if (mlab_rbf_fit(&rbf, X, y, n_pts, 2, MLAB_RBF_GAUSSIAN, 1.0) != 0) {
        test_record(0, "rbf", "dense_region_residual_1e-6", "fit fail");
        free(X); free(y);
        return 0;
    }
    test_ensure_results_dir();
    fp = fopen("results/rbf_dense_region.csv", "w");
    if (fp) fprintf(fp, "x,y,f_true,interp\n");
    for (i = 0; i < n_dense; ++i) {
        double p[2];
        double s, r;
        p[0] = -0.95 + 1.9 * mlab_rng_uniform(&rng);
        p[1] = -0.95 + 1.9 * mlab_rng_uniform(&rng);
        s = mlab_rbf_eval(&rbf, p);
        r = fabs(s - truth2d_smooth(p));
        if (r > max_res) max_res = r;
        if (fp) fprintf(fp, "%.6f,%.6f,%.8g,%.8g\n", p[0], p[1], truth2d_smooth(p), s);
    }
    if (fp) fclose(fp);
    ok = max_res < 1e-6;
    snprintf(detail, sizeof detail,
             "ε=1.0 n=%d nodes, %d dense pts: max residual=%.3e (<1e-6)",
             n_pts, n_dense, max_res);
    test_record(ok, "rbf", "dense_region_residual_1e-6", detail);
    mlab_rbf_free(&rbf);
    free(X); free(y);
    return ok;
}

static int t_kriging_matches_gp_vendor(void)
{
    /*
     * kriging 与 GP 同源（路线图 :599）：同一高斯协方差/核、同一 nugget
     * 下，简单克里金预测均值与 C9 GP 后验均值应逐点一致（不同求解路径：
     * LU vs Cholesky）。nugget→0 时 kriging 精确过节点。
     */
    const int n_pts = 15, n_query = 200;
    double X[15], y[15];
    mlab_kriging1d kr, kr0;
    mlab_gp gp;
    mlab_rng rng;
    int i, ok;
    double max_diff = 0.0, max_node = 0.0;
    char detail[200];
    FILE *fp;

    mlab_rng_seed(&rng, 73007);
    for (i = 0; i < n_pts; ++i) {
        X[i] = (double)i / (double)(n_pts - 1);
        X[i] += 0.008 * (mlab_rng_uniform(&rng) - 0.5);
        y[i] = smooth1d(X[i]) + 0.1 * mlab_rng_normal(&rng);
    }
    /* nugget = GP noise = 1e-2 */
    if (mlab_kriging1d_fit(&kr, X, y, n_pts, 0.4, 1.0, 0.01) != 0) {
        test_record(0, "kriging", "kriging_matches_gp_mean", "fit fail");
        return 0;
    }
    mlab_gp_init(&gp, 1, X, y, n_pts, 0.4, 1.0, 0.01);
    if (!mlab_gp_fit(&gp)) {
        test_record(0, "kriging", "kriging_matches_gp_mean", "gp fit fail");
        mlab_kriging1d_free(&kr);
        return 0;
    }
    test_ensure_results_dir();
    fp = fopen("results/kriging_gp_agree.csv", "w");
    if (fp) fprintf(fp, "x,kriging,gp_mean\n");
    for (i = 0; i < n_query; ++i) {
        double xq = (double)i / (double)(n_query - 1);
        double vk = mlab_kriging1d_eval(&kr, xq);
        double vg, var;
        if (!mlab_gp_predict(&gp, &xq, &vg, &var)) continue;
        if (fabs(vk - vg) > max_diff) max_diff = fabs(vk - vg);
        if (fp) fprintf(fp, "%.6f,%.8g,%.8g\n", xq, vk, vg);
    }
    if (fp) fclose(fp);
    /* nugget=1e-10 数值插值口径：n=6 良态系统上过节点（高斯核矩阵
     * 随节点数增加条件数暴涨，n=6/ls=0.3 是双精度下可验证插值性的窗口） */
    {
        double X2[6], y2[6];
        mlab_rng_seed(&rng, 73008);
        for (i = 0; i < 6; ++i) {
            X2[i] = (double)i / 5.0;
            X2[i] += 0.01 * (mlab_rng_uniform(&rng) - 0.5);
            y2[i] = smooth1d(X2[i]);
        }
        if (mlab_kriging1d_fit(&kr0, X2, y2, 6, 0.3, 1.0, 1e-10) != 0) {
            test_record(0, "kriging", "kriging_matches_gp_mean", "nugget0 fit fail");
            mlab_kriging1d_free(&kr);
            mlab_gp_release(&gp);
            return 0;
        }
        for (i = 0; i < 6; ++i) {
            double e = fabs(mlab_kriging1d_eval(&kr0, X2[i]) - y2[i]);
            if (e > max_node) max_node = e;
        }
        mlab_kriging1d_free(&kr0);
    }
    ok = (max_diff < 1e-10) && (max_node < 1e-6);
    snprintf(detail, sizeof detail,
             "max|krig-gp_mean|=%.3e over %d pts (n=15, nugget=1e-2); "
             "nugget=1e-10 node residual=%.3e (n=6)",
             max_diff, n_query, max_node);
    test_record(ok, "kriging", "kriging_matches_gp_mean", detail);
    mlab_kriging1d_free(&kr);
    mlab_kriging1d_free(&kr0);
    mlab_gp_release(&gp);
    return ok;
}

static int t_rbf_flat_kernels_illconditioned(void)
{
    /* 对照边界：极扁高斯核（ε 很小）条件数远大于中等 ε */
    const int n_pts = 20;
    double X[40], y[20];
    mlab_rbf r1, r2;
    double cond_flat, cond_mid;
    int a, b, k = 0, ok;
    char detail[180];

    for (a = 0; a < 4; ++a) {
        for (b = 0; b < 5; ++b) {
            X[k * 2 + 0] = -0.7 + 1.4 * (double)a / 3.0;
            X[k * 2 + 1] = -0.7 + 1.4 * (double)b / 4.0;
            y[k] = truth2d_smooth(&X[k * 2]);
            ++k;
        }
    }
    if (mlab_rbf_fit(&r1, X, y, n_pts, 2, MLAB_RBF_GAUSSIAN, 0.05) != 0 ||
        mlab_rbf_fit(&r2, X, y, n_pts, 2, MLAB_RBF_GAUSSIAN, 2.0) != 0) {
        test_record(0, "rbf", "flat_kernel_illconditioned", "fit fail");
        mlab_rbf_free(&r1);
        mlab_rbf_free(&r2);
        return 0;
    }
    cond_flat = mlab_rbf_cond2(&r1);
    cond_mid = mlab_rbf_cond2(&r2);
    ok = (cond_flat > 10.0 * cond_mid) || (cond_flat > 1e10);
    snprintf(detail, sizeof detail,
             "cond(ε=0.05)=%.3e vs cond(ε=2.0)=%.3e", cond_flat, cond_mid);
    test_record(ok, "rbf", "flat_kernel_illconditioned", detail);
    mlab_rbf_free(&r1);
    mlab_rbf_free(&r2);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"poly", "lagrange_exact_on_poly_deg_le_n-1",
         "Lagrange/Newton 对 deg≤n-1 多项式精确",
         t_poly_lagrange_exact_on_low_degree},
        {"poly", "newton_matches_lagrange",
         "Newton 差商与 Lagrange 在随机节点上一致",
         t_poly_newton_matches_lagrange},
        {"runge", "high_degree_blowup_vs_piecewise",
         "Runge 高次多项式误差爆炸，显著劣于分段/样条",
         t_runge_high_degree_blowup},
        {"runge", "spline_better_than_high_degree",
         "同节点数下自然三次样条优于高次 Newton",
         t_runge_spline_better_than_high_poly},
        {"spline", "natural_bc_and_node_exact",
         "自然边界 M=0，节点插值精确",
         t_spline_interpolates_nodes_and_bc},
        {"spline", "convergence_order_4",
         "三次样条实测收敛阶 4±0.3（sin(πx)）",
         t_spline_convergence_order_4},
        {"spline", "thomas_matches_a1_lu",
         "三对角 Thomas 与 A1 LU 解一致",
         t_spline_thomas_vs_a1_lu},
        {"hermite", "node_value_and_slope_match",
         "Hermite 节点值精确、导与真导一致",
         t_hermite_matches_value_and_derivative},
        {"hermite", "c1_join_continuous",
         "Hermite 相邻段在节点处 C1 连续",
         t_hermite_smooth_join},
        {"rbf", "gaussian_shape_optimal_region",
         "高斯 RBF 最优 ε 区残差<1e-6，区域外病态可复现",
         t_rbf_gaussian_shape_optimal_region},
        {"rbf", "tps_interpolates_nodes",
         "薄板样条核（含一次多项式增广）节点插值残差 < 1e-6",
         t_rbf_tps_interpolates_nodes},
        {"rbf", "tps_reproduces_affine",
         "TPS 多项式增广精确再生一次多项式",
         t_rbf_tps_reproduces_affine},
        {"rbf", "duplicate_nodes_rejected",
         "重复节点 fit 报错（-2）",
         t_rbf_duplicate_nodes_rejected},
        {"rbf", "dense_region_residual_1e-6",
         "最优区域内加密散点残差 < 1e-6（路线图 :608）",
         t_rbf_dense_region_residual},
        {"kriging", "kriging_matches_gp_mean",
         "1D kriging 与 C9 GP 后验均值同源一致；nugget=0 过节点",
         t_kriging_matches_gp_vendor},
        {"rbf", "flat_kernel_illconditioned",
         "极扁核条件数远大于中等 ε（对照）",
         t_rbf_flat_kernels_illconditioned},
    };
    test_ensure_results_dir();
    return test_run_main("D1-interp-spline", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
