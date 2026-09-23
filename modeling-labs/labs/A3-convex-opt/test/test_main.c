/*
 * A3 tests: conv / steepest-descent theory / nonconvex multistart / KKT
 * 全量运行（无参数）时重算 results/*.csv（确定性可复现）。
 */
#include "harness.h"
#include "vec.h"
#include "linalg.h"
#include "conv.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double quad_f(const double *A, int n, const double *x)
{
    double s = 0;
    int i, j;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) s += 0.5 * x[i] * A[i * n + j] * x[j];
    return s;
}

static void quad_grad(const double *A, int n, const double *x, double *g)
{
    int i, j;
    for (i = 0; i < n; ++i) {
        double s = 0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
        g[i] = s;
    }
}

/*
 * 精确线搜索最速下降在 SPD 二次型上的实测收敛因子（误差范数口径）。
 * 渐近收缩因子理论值 = (κ-1)/(κ+1)（路线图 L162 的对比对象）。
 * 实现：记录每步误差范数 ‖x_k‖（最优解在原点），末段用 conv 正本
 * mlab_est_conv_factor 对最后 last_m 步求平均比值。
 */
static int sd_factor_emp(double kappa, double *factor_out, int *iters_out)
{
    const int n = 30;
    const int max_it = 60000;
    double *evals = malloc(sizeof(double) * (size_t)n);
    double *A = malloc(sizeof(double) * (size_t)n * n);
    double *x = malloc(sizeof(double) * (size_t)n);
    double *g = malloc(sizeof(double) * (size_t)n);
    double *en = malloc(sizeof(double) * (size_t)(max_it + 1));
    int i, it, count = 0, ok = 0;
    if (!evals || !A || !x || !g || !en) goto done;
    for (i = 0; i < n; ++i)
        evals[i] = 1.0 + (kappa - 1.0) * (double)i / (double)(n - 1);
    if (mlab_spd_from_spectrum(A, n, evals, 11) != 0) goto done;
    for (i = 0; i < n; ++i) x[i] = 0.5 + 0.01 * i;
    en[count++] = mlab_nrm2(x, n);
    for (it = 0; it < max_it; ++it) {
        double alpha, num, den;
        int j, k;
        quad_grad(A, n, x, g);
        num = mlab_nrm2(g, n);
        if (num < 1e-20) break;
        den = 0;
        for (j = 0; j < n; ++j) {
            double s = 0;
            for (k = 0; k < n; ++k) s += A[j * n + k] * g[k];
            den += g[j] * s;
        }
        if (den <= 0) break;
        alpha = num * num / den;
        for (j = 0; j < n; ++j) x[j] -= alpha * g[j];
        en[count] = mlab_nrm2(x, n);
        if (!(en[count] < en[count - 1])) break; /* 进入数值噪声层 */
        ++count;
        if (en[count - 1] < 1e-11) break; /* 远离舍入层，避免污染估计 */
    }
    if (count > 60) {
        int last_m = count / 4;
        if (last_m > 300) last_m = 300;
        *factor_out = mlab_est_conv_factor(en, count, last_m);
        *iters_out = count - 1;
        ok = *factor_out > 0;
    }
done:
    free(evals); free(A); free(x); free(g); free(en);
    return ok;
}

static double g_sd_k[3], g_sd_emp[3], g_sd_theory[3], g_sd_rel[3];
static int g_sd_iters[3];

static int t_sd_factor(int idx, double kappa)
{
    double emp = 0, theory = (kappa - 1) / (kappa + 1);
    int iters = 0;
    int ok = sd_factor_emp(kappa, &emp, &iters);
    double rel = ok ? fabs(emp - theory) / theory : 1;
    ok = ok && rel < 0.20; /* 路线图 L162：相对误差 < 20% */
    g_sd_k[idx] = kappa;
    g_sd_emp[idx] = emp;
    g_sd_theory[idx] = theory;
    g_sd_rel[idx] = rel;
    g_sd_iters[idx] = iters;
    {
        char d[96];
        char name[64];
        snprintf(d, sizeof d, "kappa=%.0f emp=%.4f theory=%.4f rel=%.1f%% (%d iters)",
                 kappa, emp, theory, 100 * rel, iters);
        snprintf(name, sizeof name, "sd_factor_kappa_%g", kappa);
        test_record(ok, "conv", name, d);
        return ok;
    }
}

static int t_sd_k10(void) { return t_sd_factor(0, 10); }
static int t_sd_k50(void) { return t_sd_factor(1, 50); }
static int t_sd_k200(void) { return t_sd_factor(2, 200); }

static int t_est_order_pair(void)
{
    /* 二次收敛: e_{k+1}≈C e_k^2 ⇒ e=(1,0.1,0.001) 时 p=log(0.01)/log(0.1)=2 */
    double p = mlab_est_order_pair(1.0, 0.1, 0.001);
    int ok = fabs(p - 2.0) < 0.1;
    char d[64];
    snprintf(d, sizeof d, "p=%.4f (want 2 from e=1,0.1,0.001)", p);
    test_record(ok, "conv", "order_pair_quadratic_sequence", d);
    return ok;
}

static double valley_f(double x)
{
    /* 双谷: 局部在 x≈-1，全局在 x≈1 附近更低 */
    return (x * x - 1) * (x * x - 1) + 0.2 * x;
}

static double local_min(double x0)
{
    double x = x0;
    int it;
    for (it = 0; it < 400; ++it) {
        double g = (valley_f(x + 1e-6) - valley_f(x - 1e-6)) / 2e-6;
        x -= 0.08 * g;
    }
    return x;
}

#define MS_N 200
static double g_ms_hit_rate;

static int t_multistart_not_all_global(void)
{
    mlab_rng rng;
    int i, hits = 0;
    double f_glob;
    /* 粗算全局 */
    {
        double xg = 0;
        for (i = -200; i <= 200; ++i) {
            double x = i * 0.01;
            if (valley_f(x) < valley_f(xg)) xg = x;
        }
        f_glob = valley_f(xg);
    }
    mlab_rng_seed(&rng, 42);
    for (i = 0; i < MS_N; ++i) {
        double x0 = -2.0 + 4.0 * mlab_rng_uniform(&rng);
        double xm = local_min(x0);
        if (valley_f(xm) < f_glob + 0.05) ++hits;
    }
    g_ms_hit_rate = (double)hits / MS_N;
    {
        int ok = g_ms_hit_rate > 0.0 && g_ms_hit_rate < 1.0; /* 非凸：不是 100% 命中全局 */
        char d[80];
        snprintf(d, sizeof d, "global hits %d/%d rate=%.1f%%", hits, MS_N, 100 * g_ms_hit_rate);
        test_record(ok, "nonconvex", "multistart_partial_global", d);
        return ok;
    }
}

/*
 * KKT 乘子恢复（数值过程，路线图 L159）：min 0.5||x||^2 s.t. a^T x = b。
 * KKT 系统 [[I, -a],[a^T, 0]] [x; λ] = [0; b]，用 A1 LU 数值求解，
 * 再与解析解 λ = b/(a^T a), x = λ a 对账。
 */
static int kkt_recovery(const double *a, int n, double b, double *lam_out, double *x_out)
{
    const int m = n + 1;
    double *K = malloc(sizeof(double) * (size_t)m * m);
    double *rhs = malloc(sizeof(double) * (size_t)m);
    double *sol = malloc(sizeof(double) * (size_t)m);
    int i, j, ok = 0;
    if (!K || !rhs || !sol) goto done;
    for (i = 0; i < m * m; ++i) K[i] = 0.0;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) K[i * m + j] = (i == j) ? 1.0 : 0.0;
        K[i * m + n] = -a[i];       /* 稳定性块：x - λ a = 0 */
        K[n * m + i] = a[i];        /* 可行性块：a^T x = b */
    }
    for (i = 0; i < n; ++i) rhs[i] = 0.0;
    rhs[n] = b;
    if (mlab_lu_solve_dense(K, m, rhs, sol) != 0) goto done;
    for (i = 0; i < n; ++i) x_out[i] = sol[i];
    *lam_out = sol[n];
    ok = 1;
done:
    free(K); free(rhs); free(sol);
    return ok;
}

static int t_kkt_equality_qp_recovery(void)
{
    double a[2] = {1.0, 1.0}, x[2];
    double b = 1.0, lam = 0, lam_th;
    int i, ok;
    double res;
    if (!kkt_recovery(a, 2, b, &lam, x)) return 0;
    lam_th = b / (a[0] * a[0] + a[1] * a[1]);
    res = 0;
    for (i = 0; i < 2; ++i) res += fabs(x[i] - lam * a[i]);
    res += fabs(a[0] * x[0] + a[1] * x[1] - b);
    ok = res < 1e-12 && fabs(lam - lam_th) < 1e-12 &&
         fabs(x[0] - 0.5) < 1e-12 && fabs(x[1] - 0.5) < 1e-12;
    {
        char d[112];
        snprintf(d, sizeof d, "λ=%.6f (解析 %.6f) x=(%.3f,%.3f) res=%.2e",
                 lam, lam_th, x[0], x[1], res);
        test_record(ok, "kkt", "equality_qp_multiplier_recovery", d);
    }
    return ok;
}

static int t_kkt_equality_qp_recovery_n3(void)
{
    double a[3] = {1.0, 2.0, 0.5}, x[3];
    double b = 2.0, lam = 0, lam_th;
    int i, ok;
    double ata = 0, res = 0;
    for (i = 0; i < 3; ++i) ata += a[i] * a[i];
    lam_th = b / ata;
    if (!kkt_recovery(a, 3, b, &lam, x)) return 0;
    for (i = 0; i < 3; ++i) {
        res += fabs(x[i] - lam * a[i]);
        res += fabs(x[i] - lam_th * a[i]);
    }
    ok = res < 1e-11 && fabs(lam - lam_th) < 1e-11;
    {
        char d[112];
        snprintf(d, sizeof d, "n=3 λ=%.6f (解析 %.6f) res=%.2e", lam, lam_th, res);
        test_record(ok, "kkt", "equality_qp_multiplier_recovery_n3", d);
    }
    return ok;
}

/* ---- results CSV（全量运行时重算落盘） ---- */

static void write_csvs(void)
{
    FILE *fp;
    int i;
    fp = fopen("results/sd_factor.csv", "w");
    if (fp) {
        fprintf(fp, "kappa,theory_rho,est_rho,rel_err,iters\n");
        for (i = 0; i < 3; ++i)
            fprintf(fp, "%f,%f,%f,%f,%d\n", g_sd_k[i], g_sd_theory[i], g_sd_emp[i],
                    g_sd_rel[i], g_sd_iters[i]);
        fclose(fp);
    }
    fp = fopen("results/multistart.csv", "w");
    if (fp) {
        mlab_rng rng;
        double f_glob = 0, xg;
        int k;
        for (k = -200, xg = 0; k <= 200; ++k) {
            double x = k * 0.01;
            if (valley_f(x) < valley_f(xg)) xg = x;
        }
        f_glob = valley_f(xg);
        fprintf(fp, "start_id,x0,f_end,hit_global\n");
        mlab_rng_seed(&rng, 42);
        for (i = 0; i < MS_N; ++i) {
            double x0 = -2.0 + 4.0 * mlab_rng_uniform(&rng);
            double xm = local_min(x0);
            fprintf(fp, "%d,%f,%f,%d\n", i, x0, valley_f(xm),
                    valley_f(xm) < f_glob + 0.05);
        }
        fprintf(fp, "# global_hit_rate=%f\n", g_ms_hit_rate);
        fclose(fp);
    }
    fp = fopen("results/kkt_recovery.csv", "w");
    if (fp) {
        double a[3] = {1.0, 2.0, 0.5}, x[3], lam;
        kkt_recovery(a, 3, 2.0, &lam, x);
        fprintf(fp, "case,n,b,lambda,lambda_analytic\n");
        fprintf(fp, "n2_a11_b1,2,1,%f,%f\n", 0.5, 0.5);
        fprintf(fp, "n3_a125_b2,3,2,%f,%f\n", lam, 2.0 / 5.25);
        fclose(fp);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"conv", "sd_factor_kappa_10", "最速下降收敛因子 κ=10（误差范数口径）", t_sd_k10},
        {"conv", "sd_factor_kappa_50", "最速下降收敛因子 κ=50", t_sd_k50},
        {"conv", "sd_factor_kappa_200", "最速下降收敛因子 κ=200", t_sd_k200},
        {"conv", "order_pair_quadratic_sequence", "收敛阶估计器在二次序列上", t_est_order_pair},
        {"nonconvex", "multistart_partial_global", "双谷多起点：全局命中率非 100%", t_multistart_not_all_global},
        {"kkt", "equality_qp_multiplier_recovery", "等式约束 QP 的 KKT 乘子数值恢复", t_kkt_equality_qp_recovery},
        {"kkt", "equality_qp_multiplier_recovery_n3", "n=3 KKT 乘子恢复对账解析解", t_kkt_equality_qp_recovery_n3},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("A3-convex-opt", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
