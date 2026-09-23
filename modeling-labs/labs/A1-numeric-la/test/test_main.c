/*
 * A1 tests: linalg / numcal / numiter / matvec — 多场景覆盖
 * CLI: ./bin/test | ./bin/test linalg | ./bin/test linalg/lu_well_conditioned
 * 全量运行（无参数）时顺带把 results/*.csv 重算落盘（确定性、可复现）。
 */
#include "harness.h"
#include "vec.h"
#include "mat.h"
#include "linalg.h"
#include "numcal.h"
#include "numiter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define EPS 2.220446049250313e-16

static double f_exp(double x, void *ctx) { (void)ctx; return exp(x); }
static double f_sin(double x, void *ctx) { (void)ctx; return sin(x); }

/* ---- helpers ---- */

static void flat_matvec(const double *A, int n, const double *x, double *y)
{
    int i, j;
    for (i = 0; i < n; ++i) {
        double s = 0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
        y[i] = s;
    }
}

static int lu_one_kappa(double kappa, double *res_out)
{
    const int n = 40;
    double *sigma = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    double *xt = malloc(sizeof(double) * (size_t)n);
    double *b = malloc(sizeof(double) * (size_t)n);
    double *x = malloc(sizeof(double) * (size_t)n);
    double *r = malloc(sizeof(double) * (size_t)n);
    int i, ok = 0;
    if (!sigma || !A || !xt || !b || !x || !r) goto done;
    for (i = 0; i < n; ++i)
        sigma[i] = 1.0 + (kappa - 1.0) * (double)i / (double)(n - 1);
    if (mlab_mat_from_svd_spectrum(A, n, sigma, 7) != 0) goto done;
    for (i = 0; i < n; ++i) xt[i] = 0.1 * (i + 1);
    flat_matvec(A, n, xt, b);
    if (mlab_lu_solve_dense(A, n, b, x) != 0) goto done;
    flat_matvec(A, n, x, r);
    for (i = 0; i < n; ++i) r[i] = b[i] - r[i];
    {
        double nb = mlab_nrm2(b, n);
        *res_out = mlab_nrm2(r, n) / (nb > 0 ? nb : 1.0);
        ok = 1;
    }
done:
    free(sigma); free(A); free(xt); free(b); free(x); free(r);
    return ok;
}

/* Poisson 三对角 (-1,2,-1)，n=20；x_true=全 1 */
static void build_poisson(int n, double *A, double *b, double *x_true)
{
    int i, j;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            A[i * n + j] = (i == j) ? 2.0 : ((j == i - 1 || j == i + 1) ? -1.0 : 0.0);
    for (i = 0; i < n; ++i) x_true[i] = 1.0;
    flat_matvec(A, n, x_true, b);
}

static double res_inf(const double *A, int n, const double *x, const double *b)
{
    double *r = malloc(sizeof(double) * (size_t)n);
    double worst = 0;
    int i;
    if (!r) return 1e300;
    flat_matvec(A, n, x, r);
    for (i = 0; i < n; ++i)
        if (fabs(b[i] - r[i]) > worst) worst = fabs(b[i] - r[i]);
    free(r);
    return worst;
}

/* ---- results CSV（全量运行时重算落盘） ---- */

static void write_fd_csv(void)
{
    FILE *fp = fopen("results/fd_u_shape.csv", "w");
    int i;
    if (!fp) return;
    fprintf(fp, "h,err_forward,err_central,err_forward3\n");
    for (i = 0; i <= 20; ++i) {
        double h = pow(10.0, -16.0 + 0.75 * i);
        double ef = fabs(mlab_forward_diff(f_exp, 0.0, h, NULL) - 1.0);
        double ec = fabs(mlab_central_diff(f_exp, 0.0, h, NULL) - 1.0);
        double e3 = fabs(mlab_forward3_diff(f_exp, 0.0, h, NULL) - 1.0);
        fprintf(fp, "%.6e,%.6e,%.6e,%.6e\n", h, ef, ec, e3);
    }
    fclose(fp);
}

static void write_simpson_csv(void)
{
    FILE *fp = fopen("results/simpson_convergence.csv", "w");
    double exact = exp(1.0) - 1.0;
    int i;
    if (!fp) return;
    fprintf(fp, "n_panels,h,error,log_h,log_err\n");
    for (i = 0; i < 8; ++i) {
        int npan = 4 << i;
        double h = 1.0 / npan;
        double err = fabs(mlab_simpson(f_exp, 0.0, 1.0, npan, NULL) - exact);
        fprintf(fp, "%d,%.6e,%.6e,%.6e,%.6e\n", npan, h, err, log(h), log(err + 1e-300));
    }
    fclose(fp);
}

static void write_lu_csv(void)
{
    static const double kap[4] = {1.0, 100.0, 1e4, 1e6};
    FILE *fp = fopen("results/lu_residual.csv", "w");
    int i;
    if (!fp) return;
    fprintf(fp, "kappa,residual,threshold,pass\n");
    for (i = 0; i < 4; ++i) {
        double res = 0, thresh = 1e-12 * kap[i];
        int ok = lu_one_kappa(kap[i], &res);
        fprintf(fp, "%.0f,%.6e,%.6e,%d\n", kap[i], res, thresh, ok && res < thresh);
    }
    fclose(fp);
}

static void write_chol_csv(void)
{
    const int n = 20;
    double *evals = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    double *L = malloc(sizeof(double) * (size_t)n * n);
    double *b = malloc(sizeof(double) * (size_t)n);
    double *x = malloc(sizeof(double) * (size_t)n);
    double *xt = malloc(sizeof(double) * (size_t)n);
    FILE *fp = fopen("results/cholesky.csv", "w");
    int i, j, k;
    if (!evals || !A || !L || !b || !x || !xt || !fp) {
        free(evals); free(A); free(L); free(b); free(x); free(xt);
        if (fp) fclose(fp);
        return;
    }
    for (i = 0; i < n; ++i) evals[i] = 0.5 + i;
    if (mlab_spd_from_spectrum(A, n, evals, 3) == 0 && mlab_cholesky(A, n, L) == 0) {
        double err = 0;
        for (i = 0; i < n; ++i)
            for (j = 0; j < n; ++j) {
                double s = 0;
                for (k = 0; k < n; ++k) s += L[i * n + k] * L[j * n + k];
                err += (s - A[i * n + j]) * (s - A[i * n + j]);
            }
        err = sqrt(err / (n * n));
        for (i = 0; i < n; ++i) xt[i] = 0.1 * (i + 1);
        flat_matvec(A, n, xt, b);
        mlab_cholesky_solve(L, n, b, x);
        fprintf(fp, "frob_err,solve_rel_res\n%.6e,%.6e\n", err, res_inf(A, n, x, b));
    }
    fclose(fp);
    free(evals); free(A); free(L); free(b); free(x); free(xt);
}

static void write_numiter_csv(void)
{
    const int n = 20;
    double A[400], b[20], x[20];
    FILE *fp = fopen("results/numiter_iters.csv", "w");
    double omega_opt = 2.0 / (1.0 + sin(PI / (n + 1)));
    struct { const char *name; double omega; } rows[] = {
        {"jacobi", 0.0}, {"gauss_seidel", 0.0},
        {"sor_omega1", 1.0}, {"sor_omega_opt", omega_opt},
    };
    int r;
    if (!fp) return;
    build_poisson(n, A, b, x);
    fprintf(fp, "matrix,method,omega,iters,converged,final_res_inf\n");
    for (r = 0; r < 4; ++r) {
        int iters = 0, rc;
        int i;
        for (i = 0; i < n; ++i) x[i] = 0.0;
        if (r == 0)
            rc = mlab_jacobi_solve(A, n, b, x, 20000, 1e-12, &iters);
        else if (r == 1)
            rc = mlab_gauss_seidel_solve(A, n, b, x, 20000, 1e-12, &iters);
        else
            rc = mlab_sor_solve(A, n, b, rows[r].omega, x, 20000, 1e-12, &iters);
        fprintf(fp, "poisson20,%s,%.6f,%d,%d,%.6e\n", rows[r].name,
                rows[r].omega, iters, rc, res_inf(A, n, x, b));
    }
    fclose(fp);
}

/* ---- linalg cases ---- */

static int t_lu_well(void)
{
    double res = 1e9, thresh = 1e-12;
    int ok = lu_one_kappa(1.0, &res) && res < thresh;
    char d[128];
    snprintf(d, sizeof d, "kappa=1 residual=%.3e thresh=%.1e", res, thresh);
    test_record(ok, "linalg", "lu_well_conditioned", d);
    return ok;
}

static int t_lu_moderate(void)
{
    double res = 1e9, thresh = 1e-10;
    int ok = lu_one_kappa(1e4, &res) && res < thresh;
    char d[128];
    snprintf(d, sizeof d, "kappa=1e4 residual=%.3e (允许随 κ 放大)", res);
    test_record(ok, "linalg", "lu_moderate_condition", d);
    return ok;
}

static int t_lu_ill(void)
{
    double res = 1e9, thresh = 1e-6;
    int ok = lu_one_kappa(1e6, &res) && res < thresh;
    char d[128];
    snprintf(d, sizeof d, "kappa=1e6 residual=%.3e thresh=1e-12*kappa≈1e-6", res);
    test_record(ok, "linalg", "lu_ill_conditioned", d);
    return ok;
}

static int t_lu_singular(void)
{
    /* 两行成比例 → 奇异，应返回错误 */
    double A[4] = {1, 2, 2, 4};
    double b[2] = {1, 2};
    double x[2];
    int rc = mlab_lu_solve_dense(A, 2, b, x);
    int ok = (rc != 0);
    char d[64];
    snprintf(d, sizeof d, "singular matrix solve rc=%d (expect nonzero)", rc);
    test_record(ok, "linalg", "lu_singular_rejected", d);
    return ok;
}

static int t_chol_spd(void)
{
    const int n = 20;
    double *evals = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    double *L = malloc(sizeof(double) * (size_t)n * n);
    int i, j, k, ok = 0;
    double err = 0;
    if (!evals || !A || !L) goto done;
    for (i = 0; i < n; ++i) evals[i] = 0.5 + i;
    if (mlab_spd_from_spectrum(A, n, evals, 3) != 0) goto done;
    if (mlab_cholesky(A, n, L) != 0) goto done;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) {
            double s = 0;
            for (k = 0; k < n; ++k)
                s += L[i * n + k] * L[j * n + k]; /* L L^T */
            err += (s - A[i * n + j]) * (s - A[i * n + j]);
        }
    err = sqrt(err / (n * n));
    ok = err < 1e-12;
    {
        char d[96];
        snprintf(d, sizeof d, "||LLT-A||_F/n=%.3e (SPD)", err);
        test_record(ok, "linalg", "cholesky_spd_reconstruct", d);
    }
done:
    free(evals); free(A); free(L);
    return ok;
}

static int t_chol_not_spd(void)
{
    double A[4] = {1, 2, 2, 1}; /* 特征值 3,-1 非 SPD */
    double L[4];
    int rc = mlab_cholesky(A, 2, L);
    int ok = (rc != 0);
    char d[64];
    snprintf(d, sizeof d, "non-SPD cholesky rc=%d (expect fail)", rc);
    test_record(ok, "linalg", "cholesky_non_spd_rejected", d);
    return ok;
}

static int t_cond2_spd_matches(void)
{
    const int n = 12;
    double *evals = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    const double kappa_true = 1e4;
    double est_spd = -1, est_gen = -1;
    int i, ok = 0;
    if (!evals || !A) goto done;
    for (i = 0; i < n; ++i) evals[i] = 1.0 + (kappa_true - 1.0) * i / (n - 1);
    if (mlab_spd_from_spectrum(A, n, evals, 5) != 0) goto done;
    est_spd = mlab_spd_cond2(A, n);
    est_gen = mlab_cond2_estimate(A, n);
    ok = est_spd > 0.5e4 && est_spd < 2.0e4 &&
         est_gen > 0.5e4 && est_gen < 2.0e4;
    {
        char d[128];
        snprintf(d, sizeof d, "kappa_true=1e4 spd=%.3e general=%.3e (幂迭代估计)",
                 est_spd, est_gen);
        test_record(ok, "linalg", "cond2_estimate_spd", d);
    }
done:
    free(evals); free(A);
    return ok;
}

static int t_cond2_singular_rejected(void)
{
    double A[4] = {1, 2, 2, 4}; /* 奇异 */
    double est = mlab_cond2_estimate(A, 2);
    int ok = (est < 0);
    char d[64];
    snprintf(d, sizeof d, "singular cond2=%.3e (expect <0)", est);
    test_record(ok, "linalg", "cond2_singular_rejected", d);
    return ok;
}

/* ---- numcal cases ---- */

static int fd_scan(int use_central, double *h_star, double *err_star, int *turns)
{
    double best_h = 0, best_err = 1e300, prev = -1.0;
    int rising = 0, t = 0, i;
    *turns = 0;
    for (i = 0; i <= 20; ++i) {
        double h = pow(10.0, -16.0 + 0.75 * i);
        double d = use_central ? mlab_central_diff(f_exp, 0.0, h, NULL)
                               : mlab_forward_diff(f_exp, 0.0, h, NULL);
        double err = fabs(d - 1.0);
        if (!(err == err)) continue;
        if (err < best_err) { best_err = err; best_h = h; }
        if (prev >= 0.0 && !rising && err > prev * 1.02 + 1e-20) { rising = 1; ++t; }
        if (err == err) prev = err;
    }
    *h_star = best_h;
    *err_star = best_err;
    *turns = t;
    return best_h > 0.0;
}

static int t_fd_forward_u(void)
{
    double hs = 0, es = 0, ratio;
    int turns = 0;
    double sq = sqrt(EPS);
    int ok = fd_scan(0, &hs, &es, &turns);
    ratio = hs / sq;
    /* 路线图判据原文：比值落在 [0.3, 3] */
    ok = ok && ratio >= 0.3 && ratio <= 3.0 && turns >= 1;
    {
        char d[140];
        snprintf(d, sizeof d, "fwd h*=%.3e sqrt(eps)=%.3e ratio=%.3f turns=%d err=%.3e",
                 hs, sq, ratio, turns, es);
        test_record(ok, "numcal", "fd_forward_u_shape", d);
        return ok;
    }
}

static int t_fd_central_u(void)
{
    double hs = 0, es = 0, ratio;
    int turns = 0;
    /* 中心差分截断 O(h^2)：最优步长 ~ eps^{1/3} */
    double cube = pow(EPS, 1.0 / 3.0);
    int ok = fd_scan(1, &hs, &es, &turns);
    ratio = hs / cube;
    ok = ok && ratio >= 0.15 && ratio <= 8.0 && (turns >= 1 || es < 1e-9);
    {
        char d[140];
        snprintf(d, sizeof d, "cen h*=%.3e eps^{1/3}=%.3e ratio=%.3f turns=%d err=%.3e",
                 hs, cube, ratio, turns, es);
        test_record(ok, "numcal", "fd_central_u_shape", d);
        return ok;
    }
}

static int t_fd_methods_compare(void)
{
    /* 同一 h 下中心差分应通常优于前向差分 */
    double h = 1e-5;
    double ef = fabs(mlab_forward_diff(f_exp, 0.0, h, NULL) - 1.0);
    double ec = fabs(mlab_central_diff(f_exp, 0.0, h, NULL) - 1.0);
    int ok = ec < ef;
    char d[96];
    snprintf(d, sizeof d, "h=1e-5 err_fwd=%.3e err_cen=%.3e", ef, ec);
    test_record(ok, "numcal", "fd_forward_vs_central", d);
    return ok;
}

static int t_forward3_second_order(void)
{
    /* 三点前向差分 (-3f+4f(x+h)-f(x+2h))/(2h) 截断 O(h^2)：
       h=0.1 时误差 ≈ h^2/3 ≈ 3.3e-3，显著小于一点前向差分的 O(h)≈h/2 */
    double h = 0.1;
    double e1 = fabs(mlab_forward_diff(f_exp, 0.0, h, NULL) - 1.0);
    double e3 = fabs(mlab_forward3_diff(f_exp, 0.0, h, NULL) - 1.0);
    int ok = e3 < 5e-3 && e3 < 0.25 * e1;
    char d[96];
    snprintf(d, sizeof d, "h=0.1 err_fwd1=%.3e err_fwd3=%.3e (O(h^2))", e1, e3);
    test_record(ok, "numcal", "forward3_second_order", d);
    return ok;
}

static int t_simpson_order(void)
{
    const int m = 8;
    double ns[m], errs[m], slope = 0, intercept = 0;
    double exact = exp(1.0) - 1.0;
    int i, ok;
    for (i = 0; i < m; ++i) {
        int npan = 4 << i; /* 4,8,...,512 */
        ns[i] = log((double)npan);
        errs[i] = log(fabs(mlab_simpson(f_exp, 0.0, 1.0, npan, NULL) - exact) + 1e-300);
    }
    mlab_loglog_slope(ns, errs, m, &slope, &intercept);
    /* 路线图判据原文：实测收敛阶（log-log 回归斜率绝对值）= 4 ± 0.3 */
    ok = fabs(slope + 4.0) <= 0.3;
    {
        char d[96];
        snprintf(d, sizeof d, "log-log slope=%.4f (expect -4 ± 0.3)", slope);
        test_record(ok, "numcal", "simpson_convergence_order", d);
        return ok;
    }
}

static int t_trapezoid_vs_simpson(void)
{
    double exact = exp(1.0) - 1.0;
    int npan = 16;
    double et = fabs(mlab_trapezoid(f_exp, 0.0, 1.0, npan, NULL) - exact);
    double es = fabs(mlab_simpson(f_exp, 0.0, 1.0, npan, NULL) - exact);
    int ok = es < et;
    char d[96];
    snprintf(d, sizeof d, "n=16 err_trap=%.3e err_simp=%.3e", et, es);
    test_record(ok, "numcal", "trapezoid_vs_simpson_smooth", d);
    return ok;
}

static int t_simpson_sin(void)
{
    /* 另一被积函数：∫_0^π sin = 2 */
    double exact = 2.0;
    double est = mlab_simpson(f_sin, 0.0, PI, 256, NULL);
    double err = fabs(est - exact);
    int ok = err < 1e-8;
    char d[96];
    snprintf(d, sizeof d, "∫sin[0,π] est=%.12f err=%.3e", est, err);
    test_record(ok, "numcal", "simpson_sin_interval", d);
    return ok;
}

/* ---- numiter cases ---- */

static int t_iter_jacobi_diag_dominant(void)
{
    const int n = 6;
    double A[36], b[6], x[6], xt[6];
    int iters = 0, i, rc, ok;
    for (i = 0; i < n; ++i) {
        int j;
        for (j = 0; j < n; ++j) A[i * n + j] = (i == j) ? 6.0 : 0.5;
        xt[i] = 0.5 * (i + 1);
    }
    flat_matvec(A, n, xt, b);
    for (i = 0; i < n; ++i) x[i] = 0.0;
    rc = mlab_jacobi_solve(A, n, b, x, 500, 1e-12, &iters);
    ok = (rc == 0) && res_inf(A, n, x, b) < 1e-9 && iters > 0 && iters < 500;
    {
        char d[96];
        snprintf(d, sizeof d, "n=6 严格对角占优 iters=%d res=%.3e", iters, res_inf(A, n, x, b));
        test_record(ok, "numiter", "jacobi_diag_dominant_converges", d);
    }
    return ok;
}

static int t_iter_gs_faster_than_jacobi(void)
{
    const int n = 20;
    double A[400], b[20], xj[20], xg[20];
    int it_j = 0, it_g = 0, rcj, rcg, ok;
    build_poisson(n, A, b, xj);
    {
        int i;
        for (i = 0; i < n; ++i) { xj[i] = 0.0; xg[i] = 0.0; }
    }
    rcj = mlab_jacobi_solve(A, n, b, xj, 20000, 1e-12, &it_j);
    rcg = mlab_gauss_seidel_solve(A, n, b, xg, 20000, 1e-12, &it_g);
    /* 理论：Poisson 上 ρ_GS = ρ_J² → 迭代数约为 Jacobi 一半 */
    ok = (rcj == 0 && rcg == 0) && it_g < it_j &&
         res_inf(A, n, xg, b) < 1e-9;
    {
        char d[112];
        snprintf(d, sizeof d, "poisson20 jacobi=%d iters, GS=%d iters (ρ_GS=ρ_J²)",
                 it_j, it_g);
        test_record(ok, "numiter", "gs_faster_than_jacobi", d);
    }
    return ok;
}

static int t_iter_sor_optimal_beats_gs(void)
{
    const int n = 20;
    double A[400], b[20], xs[20], xg[20];
    double omega_opt = 2.0 / (1.0 + sin(PI / (n + 1)));
    int it_s = 0, it_g = 0, rcs, rcg, i, ok;
    build_poisson(n, A, b, xs);
    for (i = 0; i < n; ++i) { xs[i] = 0.0; xg[i] = 0.0; }
    rcs = mlab_sor_solve(A, n, b, omega_opt, xs, 20000, 1e-12, &it_s);
    rcg = mlab_gauss_seidel_solve(A, n, b, xg, 20000, 1e-12, &it_g);
    ok = (rcs == 0 && rcg == 0) && it_s < it_g && res_inf(A, n, xs, b) < 1e-9;
    {
        char d[112];
        snprintf(d, sizeof d, "poisson20 ω*=%.4f sor=%d vs GS=%d iters", omega_opt, it_s, it_g);
        test_record(ok, "numiter", "sor_optimal_beats_gs", d);
    }
    return ok;
}

static int t_iter_sor_omega1_equals_gs(void)
{
    const int n = 20;
    double A[400], b[20], xs[20], xg[20];
    int it_s = 0, it_g = 0, i, ok;
    build_poisson(n, A, b, xs);
    for (i = 0; i < n; ++i) { xs[i] = 0.0; xg[i] = 0.0; }
    mlab_sor_solve(A, n, b, 1.0, xs, 20000, 1e-12, &it_s);
    mlab_gauss_seidel_solve(A, n, b, xg, 20000, 1e-12, &it_g);
    ok = (it_s == it_g); /* ω=1 时 SOR 与 GS 迭代完全一致 */
    {
        char d[96];
        snprintf(d, sizeof d, "ω=1: sor=%d vs GS=%d (must equal)", it_s, it_g);
        test_record(ok, "numiter", "sor_omega1_matches_gs", d);
    }
    return ok;
}

static int t_iter_jacobi_diverges(void)
{
    double A[4] = {1, 2, 2, 1}; /* 谱半径 2 > 1，Jacobi 发散 */
    double b[2] = {1, 1}, x[2] = {0, 0};
    int iters = 0, rc, ok;
    double rho = mlab_jacobi_spectral_radius(A, 2);
    rc = mlab_jacobi_solve(A, 2, b, x, 50, 1e-12, &iters);
    ok = (rc == 1) && rho > 1.5;
    {
        char d[112];
        snprintf(d, sizeof d, "rho=%.4f (>1) → %d iters 后未收敛 rc=%d", rho, iters, rc);
        test_record(ok, "numiter", "jacobi_divergence_detected", d);
    }
    return ok;
}

static int t_iter_rho_poisson_theory(void)
{
    const int n = 20;
    double A[400], b[20], xt[20];
    double rho, theory = cos(PI / (n + 1));
    int ok;
    build_poisson(n, A, b, xt);
    rho = mlab_jacobi_spectral_radius(A, n);
    ok = fabs(rho - theory) < 1e-6;
    {
        char d[112];
        snprintf(d, sizeof d, "poisson20 rho=%.8f vs cos(π/(n+1))=%.8f", rho, theory);
        test_record(ok, "numiter", "jacobi_rho_matches_theory", d);
    }
    return ok;
}

/* ---- matvec cases ---- */

static int t_mat_mul_identity(void)
{
    mlab_mat A, I, C;
    int n = 5, i, ok = 0, bad = 0;
    if (mlab_mat_alloc(&A, n, n) || mlab_mat_alloc(&I, n, n) || mlab_mat_alloc(&C, n, n))
        goto done;
    for (i = 0; i < n * n; ++i) A.data[i] = 0.1 * (i + 1);
    mlab_mat_identity(&I);
    mlab_mat_mul(&C, &A, &I);
    for (i = 0; i < n * n; ++i)
        if (fabs(C.data[i] - A.data[i]) > 1e-14) ++bad;
    ok = (bad == 0);
    {
        char d[64];
        snprintf(d, sizeof d, "A*I mismatches=%d", bad);
        test_record(ok, "matvec", "mat_mul_right_identity", d);
    }
done:
    mlab_mat_free(&A); mlab_mat_free(&I); mlab_mat_free(&C);
    return ok;
}

static int t_vec_norms(void)
{
    double x[3] = {3, -4, 0};
    int ok = fabs(mlab_nrm2(x, 3) - 5.0) < 1e-14 &&
             fabs(mlab_nrm1(x, 3) - 7.0) < 1e-14 &&
             fabs(mlab_nrminf(x, 3) - 4.0) < 1e-14;
    char d[96];
    snprintf(d, sizeof d, "nrm2=%.6f nrm1=%.6f nrminf=%.6f (want 5,7,4)",
             mlab_nrm2(x, 3), mlab_nrm1(x, 3), mlab_nrminf(x, 3));
    test_record(ok, "matvec", "vec_norms_basic", d);
    return ok;
}

static int t_residual_zero(void)
{
    /* A=I, x=b → residual 0 */
    mlab_mat A;
    double x[3] = {1, 2, 3}, b[3] = {1, 2, 3};
    double r;
    int ok = 0;
    if (mlab_mat_alloc(&A, 3, 3)) return 0;
    mlab_mat_identity(&A);
    r = mlab_rel_residual(&A, x, b);
    ok = r < 1e-15;
    {
        char d[64];
        snprintf(d, sizeof d, "rel_residual=%.3e", r);
        test_record(ok, "matvec", "residual_exact_solution", d);
    }
    mlab_mat_free(&A);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"linalg", "lu_well_conditioned", "κ=1 时 LU 求解残差极小", t_lu_well},
        {"linalg", "lu_moderate_condition", "κ=1e4 中等病态残差可控", t_lu_moderate},
        {"linalg", "lu_ill_conditioned", "κ=1e6 病态仍满足 1e-12×κ 量级", t_lu_ill},
        {"linalg", "lu_singular_rejected", "奇异矩阵求解被拒绝", t_lu_singular},
        {"linalg", "cholesky_spd_reconstruct", "SPD 上 LL^T 重构", t_chol_spd},
        {"linalg", "cholesky_non_spd_rejected", "非 SPD 被拒绝", t_chol_not_spd},
        {"linalg", "cond2_estimate_spd", "谱构造 SPD 的条件数估计（两种）", t_cond2_spd_matches},
        {"linalg", "cond2_singular_rejected", "奇异矩阵条件数估计报错", t_cond2_singular_rejected},
        {"numcal", "fd_forward_u_shape", "前向差分 U 形，h*/√ε∈[0.3,3]", t_fd_forward_u},
        {"numcal", "fd_central_u_shape", "中心差分 U 形，h*~ε^{1/3}", t_fd_central_u},
        {"numcal", "fd_forward_vs_central", "中心差分优于前向差分", t_fd_methods_compare},
        {"numcal", "forward3_second_order", "三点前向差分 O(h^2) 优于一点", t_forward3_second_order},
        {"numcal", "simpson_convergence_order", "Simpson log-log 收敛阶 4±0.3", t_simpson_order},
        {"numcal", "trapezoid_vs_simpson_smooth", "光滑函数上 Simpson 更准", t_trapezoid_vs_simpson},
        {"numcal", "simpson_sin_interval", "另一被积函数 sin 区间", t_simpson_sin},
        {"numiter", "jacobi_diag_dominant_converges", "严格对角占优 Jacobi 收敛", t_iter_jacobi_diag_dominant},
        {"numiter", "gs_faster_than_jacobi", "Poisson 上 GS 迭代数≈Jacobi 一半", t_iter_gs_faster_than_jacobi},
        {"numiter", "sor_optimal_beats_gs", "最优 ω 的 SOR 快于 GS", t_iter_sor_optimal_beats_gs},
        {"numiter", "sor_omega1_matches_gs", "ω=1 时 SOR 退化为 GS", t_iter_sor_omega1_equals_gs},
        {"numiter", "jacobi_divergence_detected", "谱半径>1 时 Jacobi 发散", t_iter_jacobi_diverges},
        {"numiter", "jacobi_rho_matches_theory", "Poisson 谱半径 = cos(π/(n+1))", t_iter_rho_poisson_theory},
        {"matvec", "mat_mul_right_identity", "矩阵乘单位阵", t_mat_mul_identity},
        {"matvec", "vec_norms_basic", "向量三种范数", t_vec_norms},
        {"matvec", "residual_exact_solution", "精确解相对残差≈0", t_residual_zero},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("A1-numeric-la", cases, (int)(sizeof cases / sizeof cases[0]), argc, argv);
    /* 全量运行且未带过滤器时重算 results 数据（确定性，可复现） */
    if (argc <= 1) {
        write_fd_csv();
        write_simpson_csv();
        write_lu_csv();
        write_chol_csv();
        write_numiter_csv();
    }
    return rc;
}
