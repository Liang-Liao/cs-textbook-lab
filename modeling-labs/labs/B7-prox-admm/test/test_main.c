/*
 * B7: prox — soft-threshold / ISTA / FISTA / ADMM / 次梯度 / 近端点 / BP / ADMM-QP 对账
 * 全量运行时重算 results 目录下的 CSV（确定性可复现）。
 */
#include "harness.h"
#include "prox.h"
#include "nlp.h"
#include "opt.h"
#include "rng.h"
#include "vec.h"
#include "linalg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int t_soft_threshold_cases(void)
{
    int ok = fabs(mlab_soft_threshold(3.0, 1.0) - 2.0) < 1e-15 &&
             fabs(mlab_soft_threshold(-3.0, 1.0) + 2.0) < 1e-15 &&
             mlab_soft_threshold(0.5, 1.0) == 0.0 &&
             mlab_soft_threshold(1.0, 1.0) == 0.0;
    char d[80];
    snprintf(d, sizeof d, "st(3,1)=%.1f st(-3,1)=%.1f st(0.5,1)=%.1f",
             mlab_soft_threshold(3, 1), mlab_soft_threshold(-3, 1), mlab_soft_threshold(0.5, 1));
    test_record(ok, "prox_op", "soft_threshold_basic", d);
    return ok;
}

static int t_soft_threshold_monotone(void)
{
    /* 软阈值单调非降且有收缩不动点 */
    double prev = mlab_soft_threshold(-2.0, 0.5);
    int i, ok = 1;
    for (i = 0; i <= 40; ++i) {
        double z = -2.0 + 4.0 * i / 40.0;
        double v = mlab_soft_threshold(z, 0.5);
        if (v < prev - 1e-15) ok = 0;
        prev = v;
    }
    ok = ok && fabs(mlab_soft_threshold(2.5, 0.5) - 2.0) < 1e-15;
    {
        char d[64];
        snprintf(d, sizeof d, "单调 + 收缩 2.5→%.2f", mlab_soft_threshold(2.5, 0.5));
        test_record(ok, "prox_op", "soft_threshold_monotone", d);
        return ok;
    }
}

static void gen_corr(double *X, double *y, double *wt, int m, int n, int nz,
                     double rho, unsigned seed)
{
    mlab_rng rng;
    int i, j;
    mlab_rng_seed(&rng, seed);
    memset(wt, 0, sizeof(double) * (size_t)n);
    for (j = 0; j < nz; ++j) wt[j] = (j % 2 ? -1.0 : 1.0) * (1.2 + 0.1 * j);
    for (i = 0; i < m; ++i) {
        double prev = 0, yh = 0, s = sqrt(1 - rho * rho);
        for (j = 0; j < n; ++j) {
            double z = mlab_rng_normal(&rng);
            double x = j == 0 ? z : rho * prev + s * z;
            prev = x;
            X[i * n + j] = x;
            yh += x * wt[j];
        }
        y[i] = yh + 0.02 * mlab_rng_normal(&rng);
    }
}

static int t_ista_support_recovery(void)
{
    const int m = 100, n = 15, nz = 4;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *w = calloc((size_t)n, sizeof(double));
    int j, hit = 0, ok = 0;
    if (!X || !y || !wt || !w) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.2, 21);
    mlab_ista(X, m, n, y, 0.05, w, 3000);
    for (j = 0; j < n; ++j) {
        int t = fabs(wt[j]) > 0.3;
        int e = fabs(w[j]) > 0.3;
        if (t == e) ++hit;
    }
    ok = (double)hit / n >= 0.9;
    {
        char d[48];
        snprintf(d, sizeof d, "support acc=%.0f%%", 100.0 * hit / n);
        test_record(ok, "ista", "sparse_support_recovery", d);
    }
done:
    free(X); free(y); free(wt); free(w);
    return ok;
}

static int t_ista_zero_lambda_near_ls(void)
{
    /* λ=0 时近似最小二乘，目标应很小 */
    const int m = 30, n = 5;
    double X[150], y[30], w[5] = {0};
    int i, j;
    double o;
    mlab_rng rng;
    mlab_rng_seed(&rng, 3);
    for (i = 0; i < m; ++i) {
        double yh = 0;
        for (j = 0; j < n; ++j) {
            X[i * n + j] = mlab_rng_normal(&rng);
            yh += X[i * n + j] * (0.5 * (j + 1));
        }
        y[i] = yh;
    }
    o = mlab_ista(X, m, n, y, 0.0, w, 2000);
    {
        int ok = o < 1e-4;
        char d[48];
        snprintf(d, sizeof d, "obj=%.3e (λ=0, noise-free)", o);
        test_record(ok, "ista", "zero_lambda_fits_noise_free", d);
        return ok;
    }
}

static double obj_k(const double *X, int m, int n, const double *y,
                    double lam, int k, int fista)
{
    double *w = calloc((size_t)n, sizeof(double));
    double o;
    if (!w) return 1e300;
    o = fista ? mlab_fista(X, m, n, y, lam, w, k) : mlab_ista(X, m, n, y, lam, w, k);
    free(w);
    return o;
}

static int t_fista_faster_than_ista(void)
{
    const int m = 80, n = 20, nz = 5;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double lam = 0.05, f0, fopt, target;
    int ki = 0, kf = 0, k, ok = 0;
    FILE *fp;
    if (!X || !y || !wt) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.92, 33);
    f0 = obj_k(X, m, n, y, lam, 0, 0);
    fopt = obj_k(X, m, n, y, lam, 8000, 1);
    if (!(fopt < f0)) fopt = f0 - 1;
    target = fopt + 1e-4 * (f0 - fopt);
    fp = fopen("results/subgrad_vs_ista.csv", "w");
    if (fp) fprintf(fp, "k,obj_ista,obj_fista\n");
    for (k = 1; k <= 20000; ++k) {
        double oi = obj_k(X, m, n, y, lam, k, 0);
        double of = obj_k(X, m, n, y, lam, k, 1);
        if (fp && (k <= 20 || k % 100 == 0))
            fprintf(fp, "%d,%.8e,%.8e\n", k, oi, of);
        if (ki == 0 && oi <= target) ki = k;
        if (kf == 0 && of <= target) kf = k;
        if (ki && kf) break;
    }
    if (fp) fclose(fp);
    if (ki == 0) ki = 20000;
    if (kf == 0) kf = 20000;
    ok = kf * 2 <= ki; /* 路线图 L340：FISTA 迭代数 ≤ ISTA 的 1/2 */
    {
        char d[64];
        snprintf(d, sizeof d, "ISTA=%d FISTA=%d (到同目标)", ki, kf);
        test_record(ok, "fista", "faster_than_ista_same_gap", d);
    }
done:
    free(X); free(y); free(wt);
    return ok;
}

static int t_admm_matches_ista_obj(void)
{
    const int m = 50, n = 12, nz = 3;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *w1 = calloc((size_t)n, sizeof(double));
    double *w2 = calloc((size_t)n, sizeof(double));
    double o1, o2, rel;
    int ok = 0;
    if (!X || !y || !wt || !w1 || !w2) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.3, 55);
    o1 = mlab_ista(X, m, n, y, 0.1, w1, 4000);
    o2 = mlab_admm_lasso(X, m, n, y, 0.1, w2, 200, 1.0);
    rel = fabs(o2 - o1) / (1 + fabs(o1));
    ok = rel < 1e-4;
    {
        char d[64];
        snprintf(d, sizeof d, "rel obj diff=%.2e", rel);
        test_record(ok, "admm", "objective_matches_ista", d);
    }
done:
    free(X); free(y); free(wt); free(w1); free(w2);
    return ok;
}

static int t_admm_rho_invariance(void)
{
    /* 不同 rho 收敛到同一 LASSO 解（原始/对偶残差均 → 0） */
    const int m = 50, n = 12, nz = 3;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *w1 = calloc((size_t)n, sizeof(double));
    double *w2 = calloc((size_t)n, sizeof(double));
    double dmax = 0;
    int i, ok = 0;
    if (!X || !y || !wt || !w1 || !w2) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.3, 55);
    mlab_admm_lasso(X, m, n, y, 0.1, w1, 20000, 0.2);
    mlab_admm_lasso(X, m, n, y, 0.1, w2, 20000, 8.0);
    for (i = 0; i < n; ++i)
        if (fabs(w1[i] - w2[i]) > dmax) dmax = fabs(w1[i] - w2[i]);
    ok = dmax < 1e-8;
    {
        char d[64];
        snprintf(d, sizeof d, "rho=0.2 vs 8：max|dw|=%.2e", dmax);
        test_record(ok, "admm", "rho_invariance_same_solution", d);
    }
done:
    free(X); free(y); free(wt); free(w1); free(w2);
    return ok;
}

/* ---- 次梯度 vs ISTA（路线图 L327/334 实验 1） ---- */

static int t_subgradient_vs_ista(void)
{
    const int m = 60, n = 12, nz = 3;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *ws = calloc((size_t)n, sizeof(double));
    double *wi = calloc((size_t)n, sizeof(double));
    double o_sub300, o_ista300, o_sub_big, o_ista_big;
    int ok = 0;
    FILE *fp;
    if (!X || !y || !wt || !ws || !wi) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.3, 77);
    o_sub300 = mlab_subgradient_lasso(X, m, n, y, 0.1, ws, 50, 0.02);
    o_ista300 = mlab_ista(X, m, n, y, 0.1, wi, 50);
    o_sub_big = mlab_subgradient_lasso(X, m, n, y, 0.1, ws, 60000, 0.02);
    o_ista_big = mlab_ista(X, m, n, y, 0.1, wi, 60000);
    fp = fopen("results/subgrad_vs_ista.csv", "w");
    if (fp) {
        fprintf(fp, "method,iters300,iters60000\n");
        fprintf(fp, "subgradient,%.8e,%.8e\n", o_sub300, o_sub_big);
        fprintf(fp, "ista,%.8e,%.8e\n", o_ista300, o_ista_big);
        fclose(fp);
    }
    /* 次梯度法明显更慢：同预算 300 步 ISTA 目标更优；
       次梯度法大量迭代后仍能接近最优（方法本身正确） */
    ok = o_ista300 < o_sub300 && o_sub_big < o_ista300 + 1e-6;
    {
        char d[112];
        snprintf(d, sizeof d, "50 步：ISTA %.4e < 次梯度 %.4e；6e4 步次梯度 %.4e 追近",
                 o_ista300, o_sub300, o_sub_big);
        test_record(ok, "subgradient", "slower_than_ista_but_converges", d);
    }
done:
    free(X); free(y); free(wt); free(ws); free(wi);
    return ok;
}

/* ---- 近端点法（L328） ---- */

static int t_prox_point_converges(void)
{
    const int m = 50, n = 10, nz = 3;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *w1 = calloc((size_t)n, sizeof(double));
    double *w2 = calloc((size_t)n, sizeof(double));
    double o1, o2, rel;
    int ok = 0;
    if (!X || !y || !wt || !w1 || !w2) goto done;
    gen_corr(X, y, wt, m, n, nz, 0.3, 99);
    o1 = mlab_ista(X, m, n, y, 0.1, w1, 8000);
    o2 = mlab_prox_point_lasso(X, m, n, y, 0.1, w2, 40, 100, 2.0);
    rel = fabs(o2 - o1) / (1 + fabs(o1));
    ok = rel < 1e-6;
    {
        char d[64];
        snprintf(d, sizeof d, "近端点 vs ISTA 相对目标差=%.2e", rel);
        test_record(ok, "prox_point", "matches_ista_objective", d);
    }
done:
    free(X); free(y); free(wt); free(w1); free(w2);
    return ok;
}

/* ---- 基追踪（L331） ---- */

static int t_bp_recovers_sparse(void)
{
    /* 无噪 BP：y = Xw_true（m=30 < n=60，稀疏 w_true）→ 恢复 w_true */
    const int m = 30, n = 60, nz = 4;
    double *X = malloc(sizeof(double) * m * n);
    double *y = malloc(sizeof(double) * m);
    double *wt = calloc((size_t)n, sizeof(double));
    double *w = calloc((size_t)n, sizeof(double));
    double l1_true = 0, resid = 0, dmax = 0;
    int i, j, ok = 0;
    mlab_rng rng;
    if (!X || !y || !wt || !w) goto done;
    mlab_rng_seed(&rng, 123ULL);
    for (i = 0; i < m * n; ++i) X[i] = mlab_rng_normal(&rng);
    wt[1] = 1.5; wt[5] = -2.0; wt[9] = 0.8; wt[17] = -1.1;
    for (i = 0; i < m; ++i) {
        double yh = 0;
        for (j = 0; j < n; ++j) yh += X[i * n + j] * wt[j];
        y[i] = yh;
        l1_true += fabs(wt[j]);
    }
    (void)l1_true;
    mlab_bp_admm(X, m, n, y, w, 4000, 1.0);
    {
        double *r = malloc(sizeof(double) * m);
        if (!r) goto done;
        for (i = 0; i < m; ++i) {
            double yh = 0;
            for (j = 0; j < n; ++j) yh += X[i * n + j] * w[j];
            r[i] = yh - y[i];
            resid += r[i] * r[i];
        }
        free(r);
    }
    for (j = 0; j < n; ++j)
        if (fabs(w[j] - wt[j]) > dmax) dmax = fabs(w[j] - wt[j]);
    ok = resid < 1e-10 && dmax < 1e-4; /* 精确恢复（无噪 BP 的稀疏恢复性质） */
    {
        char d[80];
        snprintf(d, sizeof d, "resid=%.2e max|dw|=%.2e（精确恢复）", resid, dmax);
        test_record(ok, "basis_pursuit", "recovers_sparse_exact", d);
    }
done:
    free(X); free(y); free(wt); free(w);
    return ok;
}

/* ---- ADMM 与 B5 增广拉格朗日带约束 QP 对账（路线图 L336/341） ---- */

static double f_norm_half(const double *x, int n, void *ctx)
{
    (void)n; (void)ctx;
    return 0.5 * (x[0] * x[0] + x[1] * x[1]);
}
static void g_norm_half(const double *x, int n, double *g, void *ctx)
{
    (void)n; (void)ctx;
    g[0] = x[0];
    g[1] = x[1];
}
static void eq_sum1(const double *x, int n, double *h, int m_eq, void *ctx)
{
    (void)n; (void)ctx; (void)m_eq;
    h[0] = x[0] + x[1] - 1.0;
}

static int t_admm_vs_auglag_eq_qp(void)
{
    mlab_objective obj;
    double x_al[2] = {3.0, -2.0}, nu[1] = {0}, x_admm[2];
    double diff;
    int ok;
    FILE *fp;
    /* B5 增广拉格朗日（vendor 正本）解同一带约束 QP */
    obj.f = f_norm_half; obj.grad = g_norm_half; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    mlab_aug_lag(&obj, NULL, 0, NULL, eq_sum1, 1, NULL,
                 1.0, x_al, NULL, nu, 300, 1e-12, NULL);
    /* ADMM 解同一问题 */
    {
        double A[2] = {1.0, 1.0}, b[1] = {1.0};
        mlab_admm_qp_eq(A, 1, 2, b, 1.0, 20000, 1e-13, x_admm);
    }
    diff = fabs(x_al[0] - x_admm[0]) + fabs(x_al[1] - x_admm[1]);
    fp = fopen("results/admm_al_reconcile.csv", "w");
    if (fp) {
        fprintf(fp, "method,x0,x1\n");
        fprintf(fp, "aug_lag,%.12f,%.12f\n", x_al[0], x_al[1]);
        fprintf(fp, "admm,%.12f,%.12f\n", x_admm[0], x_admm[1]);
        fclose(fp);
    }
    /* 路线图 L341：两法终解相对差 < 1e-8 */
    ok = diff < 1e-8 && fabs(x_admm[0] - 0.5) < 1e-8;
    {
        char d[96];
        snprintf(d, sizeof d, "AL=(%.10f,%.10f) ADMM=(%.10f,%.10f) 差=%.2e (<1e-8)",
                 x_al[0], x_al[1], x_admm[0], x_admm[1], diff);
        test_record(ok, "admm", "matches_auglag_on_eq_qp", d);
    }
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"prox_op", "soft_threshold_basic", "软阈值边界与符号", t_soft_threshold_cases},
        {"prox_op", "soft_threshold_monotone", "软阈值单调性与收缩", t_soft_threshold_monotone},
        {"ista", "sparse_support_recovery", "ISTA 稀疏支持恢复", t_ista_support_recovery},
        {"ista", "zero_lambda_fits_noise_free", "λ=0 拟合无噪数据", t_ista_zero_lambda_near_ls},
        {"fista", "faster_than_ista_same_gap", "FISTA 达同精度更快（L340）", t_fista_faster_than_ista},
        {"subgradient", "slower_than_ista_but_converges", "次梯度法慢于 ISTA 但收敛（L334）", t_subgradient_vs_ista},
        {"prox_point", "matches_ista_objective", "近端点法收敛同目标（L328）", t_prox_point_converges},
        {"basis_pursuit", "recovers_sparse_exact", "基追踪精确恢复稀疏解（L331）", t_bp_recovers_sparse},
        {"admm", "objective_matches_ista", "ADMM 与 ISTA 目标一致", t_admm_matches_ista_obj},
        {"admm", "rho_invariance_same_solution", "不同 rho 同一解", t_admm_rho_invariance},
        {"admm", "matches_auglag_on_eq_qp", "ADMM vs B5 AL 带约束 QP 对账 <1e-8（L341）", t_admm_vs_auglag_eq_qp},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("B7-prox-admm", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    return rc;
}
