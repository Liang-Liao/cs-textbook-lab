/*
 * B5: penalty / aug-lag / proj-grad / box_qp / log-barrier / sqp
 * 全量运行（无参数）时重算 results/*.csv（固定 seed，确定性可复现）。
 */
#include "harness.h"
#include "nlp.h"
#include "box_qp.h"
#include "opt.h"
#include "vec.h"
#include "linalg.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double f_quad_generic(const double *x, int n, void *ctx);
static void g_quad_generic(const double *x, int n, double *g, void *ctx);

/* min 0.5*(x-2)^2 s.t. x <= 1 → x*=1 */
static double f_shift(const double *x, int n, void *ctx)
{
    (void)n; (void)ctx;
    return 0.5 * (x[0] - 2.0) * (x[0] - 2.0);
}
static void g_shift(const double *x, int n, double *g, void *ctx)
{
    (void)n; (void)ctx;
    g[0] = x[0] - 2.0;
}
static void ineq_x_le_1(const double *x, int n, double *g, int m_ineq, void *ctx)
{
    (void)n; (void)ctx; (void)m_ineq;
    g[0] = x[0] - 1.0; /* g<=0 */
}

/* min 0.5||x||^2 s.t. x1+x2=1 → x*=(0.5,0.5) */
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

static int t_penalty_pushes_to_boundary(void)
{
    mlab_objective obj;
    double x[1] = {0.0}, kkt = 0;
    int ok;
    obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
    ok = mlab_penalty_outer(&obj, ineq_x_le_1, 1, NULL, NULL, 0, NULL,
                            1e3, x, 200, 1e-8, &kkt) == 0;
    ok = ok && fabs(x[0] - 1.0) < 0.05 && kkt < 1e-6;
    {
        char d[64];
        snprintf(d, sizeof d, "x=%.4f want ≈1 kkt=%.2e", x[0], kkt);
        test_record(ok, "penalty", "large_mu_near_active_bound", d);
        return ok;
    }
}

/*
 * 路线图 L287 判据：增广拉格朗日达到与纯罚相同解精度所需罚系数
 * ≤ 纯罚方案的 1%。目标精度 ε=1e-4，扫描各自的最小罚/σ。
 */
static double g_mu_min, g_sigma_min;

static double solve_penalty_err(double mu)
{
    mlab_objective obj;
    double x[1] = {0.0};
    obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
    mlab_penalty_outer(&obj, ineq_x_le_1, 1, NULL, NULL, 0, NULL,
                       mu, x, 1, 1e-12, NULL); /* 单轮：固定 μ 求解 */
    return fabs(x[0] - 1.0);
}

static double solve_al_err(double sigma)
{
    mlab_objective obj;
    double x[1] = {0.0}, lam[1] = {0}, nu[1] = {0};
    obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
    mlab_aug_lag(&obj, ineq_x_le_1, 1, NULL, NULL, 0, NULL,
                 sigma, x, lam, nu, 50, 1e-12, NULL);
    return fabs(x[0] - 1.0);
}

static int t_penalty_vs_al_mu_scan(void)
{
    const double eps = 1e-4;
    double mu, sigma;
    int i, ok;
    FILE *fp;
    g_mu_min = -1;
    g_sigma_min = -1;
    fp = fopen("results/pen_vs_al.csv", "w");
    if (fp) fprintf(fp, "method,mu_or_sigma,err\n");
    for (i = 0; i <= 6; ++i) {
        double err;
        mu = pow(10.0, i);
        err = solve_penalty_err(mu);
        if (g_mu_min < 0 && err <= eps) g_mu_min = mu;
        if (fp) fprintf(fp, "penalty,%.0e,%.3e\n", mu, err);
    }
    for (i = 0; i <= 6; ++i) {
        double err;
        sigma = pow(4.0, i);
        err = solve_al_err(sigma);
        if (g_sigma_min < 0 && err <= eps) g_sigma_min = sigma;
        if (fp) fprintf(fp, "auglag,%.0e,%.3e\n", sigma, err);
    }
    if (fp) fclose(fp);
    /* 判据：σ_min ≤ 0.01 × μ_min（1%） */
    ok = g_mu_min > 0 && g_sigma_min > 0 && g_sigma_min <= 0.01 * g_mu_min;
    {
        char d[112];
        snprintf(d, sizeof d, "ε=1e-4: μ_min=%.0e vs σ_min=%.0f（比值 %.2f%% ≤ 1%%）",
                 g_mu_min, g_sigma_min,
                 g_mu_min > 0 ? 100.0 * g_sigma_min / g_mu_min : 100.0);
        test_record(ok, "aug_lag", "al_needs_1pct_of_penalty_mu", d);
    }
    return ok;
}

static int t_auglag_equality_constraint(void)
{
    mlab_objective obj;
    double x[2] = {3.0, -2.0}, lam[1] = {0}, nu[1] = {0}, kkt = 0;
    int ok;
    obj.f = f_norm_half; obj.grad = g_norm_half; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    ok = mlab_aug_lag(&obj, NULL, 0, NULL, eq_sum1, 1, NULL,
                      1.0, x, lam, nu, 100, 1e-9, &kkt) == 0;
    ok = ok && fabs(x[0] - 0.5) < 1e-4 && fabs(x[1] - 0.5) < 1e-4 && kkt < 1e-6;
    {
        char d[80];
        snprintf(d, sizeof d, "x=(%.5f,%.5f) ν=%.4f kkt=%.2e want (0.5,0.5)",
                 x[0], x[1], nu[0], kkt);
        test_record(ok, "aug_lag", "equality_constraint_multiplier", d);
        return ok;
    }
}

static double f_box(const double *x, int n, void *ctx)
{
    (void)n; (void)ctx;
    return 0.5 * (x[0] * x[0] + x[1] * x[1]) - 3.0 * x[0] - 2.0 * x[1];
}
static void g_box(const double *x, int n, double *g, void *ctx)
{
    (void)n; (void)ctx;
    g[0] = x[0] - 3.0;
    g[1] = x[1] - 2.0;
}

static int t_proj_grad_box_clamp(void)
{
    mlab_objective obj;
    double lb[2] = {0, 0}, ub[2] = {1, 1}, x[2] = {0.5, 0.5}, kkt = 0;
    int ok;
    obj.f = f_box; obj.grad = g_box; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    ok = mlab_proj_grad_box(&obj, lb, ub, x, 500, 1e-10, &kkt) == 0;
    /* unconstrained min at (3,2) → clamp to (1,1) */
    ok = ok && fabs(x[0] - 1) < 1e-8 && fabs(x[1] - 1) < 1e-8 && kkt < 1e-8;
    {
        char d[64];
        snprintf(d, sizeof d, "x=(%.6f,%.6f) kkt=%.1e want (1,1)", x[0], x[1], kkt);
        test_record(ok, "proj_grad", "box_clamp_unconstrained_outside", d);
        return ok;
    }
}

/* 有效集枚举（2 维箱式 QP 的精确解，测试侧基准） */
static int enum_box_qp(const double *Q, const double *c, const double *lb,
                       const double *ub, double *x_out, double *f_out)
{
    int a0, a1, best = 0;
    double bf = 1e300;
    for (a0 = -1; a0 <= 1; ++a0)
        for (a1 = -1; a1 <= 1; ++a1) {
            /* a=-1 free, 0 at lb, 1 at ub */
            int nf = (a0 == -1) + (a1 == -1);
            double xf[2], x[2], f;
            int ok = 1;
            if (nf == 2) {
                /* 解 Q x = -c */
                double A[4] = {Q[0], Q[1], Q[2], Q[3]}, rhs[2] = {-c[0], -c[1]};
                if (mlab_lu_solve_dense(A, 2, rhs, xf) != 0) ok = 0;
                x[0] = xf[0]; x[1] = xf[1];
            } else if (nf == 1) {
                int freei = (a0 == -1) ? 0 : 1;
                int bndi = 1 - freei;
                double bnd = (bndi == 0) ? ((a0 == 0) ? lb[0] : ub[0])
                                         : ((a1 == 0) ? lb[1] : ub[1]);
                double q = Q[freei * 2 + freei];
                double off = Q[freei * 2 + bndi] * bnd;
                xf[0] = (-c[freei] - off) / q;
                if (freei == 0) { x[0] = xf[0]; x[1] = bnd; }
                else { x[1] = xf[0]; x[0] = bnd; }
                (void)bndi;
            } else {
                x[0] = (a0 == 0) ? lb[0] : ub[0];
                x[1] = (a1 == 0) ? lb[1] : ub[1];
            }
            if (!ok) continue;
            if (x[0] < lb[0] - 1e-9 || x[0] > ub[0] + 1e-9) continue;
            if (x[1] < lb[1] - 1e-9 || x[1] > ub[1] + 1e-9) continue;
            /* KKT 符号检查（凸 QP） */
            {
                double g0 = Q[0] * x[0] + Q[1] * x[1] + c[0];
                double g1 = Q[2] * x[0] + Q[3] * x[1] + c[1];
                if (a0 == 0 && g0 < -1e-9) continue; /* 在下界但还想更小 → 非最优 */
                if (a0 == 1 && g0 > 1e-9) continue;
                if (a1 == 0 && g1 < -1e-9) continue;
                if (a1 == 1 && g1 > 1e-9) continue;
            }
            f = 0.5 * (x[0] * (Q[0] * x[0] + Q[1] * x[1]) +
                       x[1] * (Q[2] * x[0] + Q[3] * x[1])) + c[0] * x[0] + c[1] * x[1];
            if (f < bf) {
                bf = f;
                x_out[0] = x[0];
                x_out[1] = x[1];
                best = 1;
            }
        }
    if (f_out && best) *f_out = bf;
    return best ? 0 : 1;
}

static int g_box_rand_mismatch;

static int t_proj_grad_vs_active_set_random(void)
{
    mlab_rng rng;
    int trial, mismatch = 0, ok;
    FILE *fp;
    const double lb[2] = {-1, -1}, ub[2] = {1, 1};
    fp = fopen("results/box_rand100.csv", "w");
    if (fp) fprintf(fp, "trial,agree\n");
    mlab_rng_seed(&rng, 9);
    for (trial = 0; trial < 100; ++trial) {
        double Q[4], c[2], xas[2], xpg[2] = {0, 0}, kkt = 0, oas = 0;
        mlab_objective obj;
        double a = 0.5 + mlab_rng_uniform(&rng);
        double b = 0.1 * (mlab_rng_uniform(&rng) - 0.5);
        double d = 0.5 + mlab_rng_uniform(&rng);
        Q[0] = a; Q[1] = b; Q[2] = b; Q[3] = d;
        c[0] = -2.0 + 4.0 * mlab_rng_uniform(&rng);
        c[1] = -2.0 + 4.0 * mlab_rng_uniform(&rng);
        if (enum_box_qp(Q, c, lb, ub, xas, &oas) != 0) { ++mismatch; continue; }
        obj.f = f_quad_generic; obj.grad = g_quad_generic;
        obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
        {
            double qc[6];
            qc[0] = Q[0]; qc[1] = Q[1]; qc[2] = Q[2]; qc[3] = Q[3];
            qc[4] = c[0]; qc[5] = c[1];
            obj.ctx = qc;
            mlab_proj_grad_box(&obj, lb, ub, xpg, 2000, 1e-10, &kkt);
        }
        {
            int agree = fabs(xpg[0] - xas[0]) < 1e-5 && fabs(xpg[1] - xas[1]) < 1e-5;
            if (!agree) ++mismatch;
            if (fp) fprintf(fp, "%d,%d\n", trial, agree);
        }
    }
    if (fp) fclose(fp);
    g_box_rand_mismatch = mismatch;
    ok = mismatch == 0; /* 路线图 L288：100 个随机箱式问题一致 */
    {
        char d[80];
        snprintf(d, sizeof d, "100 个随机箱式 QP：投影梯度 vs 有效集枚举 不一致=%d", mismatch);
        test_record(ok, "proj_grad", "agrees_active_set_100_random", d);
    }
    return ok;
}

static double f_quad_generic(const double *x, int n, void *ctx)
{
    const double *qc = ctx;
    (void)n;
    return 0.5 * (x[0] * (qc[0] * x[0] + qc[1] * x[1]) +
                  x[1] * (qc[2] * x[0] + qc[3] * x[1])) + qc[4] * x[0] + qc[5] * x[1];
}

static void g_quad_generic(const double *x, int n, double *g, void *ctx)
{
    const double *qc = ctx;
    (void)n;
    g[0] = qc[0] * x[0] + qc[1] * x[1] + qc[4];
    g[1] = qc[2] * x[0] + qc[3] * x[1] + qc[5];
}

static int t_box_qp_active_set(void)
{
    /* min 0.5 x'Qx + c'x, Q=I, c=(-3,-2), box [0,1]^2 → (1,1) */
    double Q[4] = {1, 0, 0, 1}, c[2] = {-3, -2};
    double lb[2] = {0, 0}, ub[2] = {1, 1}, x[2], objv = 0;
    int ok = mlab_box_qp_active_set(Q, c, lb, ub, 2, x, &objv) == 0;
    ok = ok && fabs(x[0] - 1) < 1e-8 && fabs(x[1] - 1) < 1e-8;
    {
        char d[64];
        snprintf(d, sizeof d, "x=(%.4f,%.4f) obj=%.4f", x[0], x[1], objv);
        test_record(ok, "box_qp", "active_set_hits_upper_bounds", d);
        return ok;
    }
}

static int t_box_qp_interior(void)
{
    /* 最优在盒内：Q=2I, c=(1,-1), box [-2,2]^2 → x=(-0.5, 0.5) */
    double Q[4] = {2, 0, 0, 2}, c[2] = {1, -1};
    double lb[2] = {-2, -2}, ub[2] = {2, 2}, x[2], objv = 0;
    int ok = mlab_box_qp_active_set(Q, c, lb, ub, 2, x, &objv) == 0;
    ok = ok && fabs(x[0] + 0.5) < 1e-8 && fabs(x[1] - 0.5) < 1e-8;
    {
        char d[64];
        snprintf(d, sizeof d, "x=(%.4f,%.4f) obj=%.4f want (-0.5,0.5)", x[0], x[1], objv);
        test_record(ok, "box_qp", "interior_solution", d);
        return ok;
    }
}

/* ---- 内点法（对数障碍）与 SQP ---- */

static int t_log_barrier_converges(void)
{
    mlab_objective obj;
    double x[1] = {0.5}, kkt = 0;
    int ok;
    obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
    ok = mlab_log_barrier(&obj, ineq_x_le_1, 1, NULL, 1.0, x, 30, 1e-7, &kkt) == 0;
    ok = ok && fabs(x[0] - 1.0) < 1e-3 && kkt < 1e-5;
    {
        char d[80];
        snprintf(d, sizeof d, "x=%.6f kkt=%.2e want ≈1（λ=μ/(−g)）", x[0], kkt);
        test_record(ok, "interior", "log_barrier_converges", d);
        return ok;
    }
}

static int t_sqp_equality_qp(void)
{
    mlab_objective obj;
    double x[2] = {3.0, -2.0}, kkt = 0;
    int ok;
    obj.f = f_norm_half; obj.grad = g_norm_half; obj.hess = NULL; obj.dim = 2; obj.ctx = NULL;
    ok = mlab_sqp_min(&obj, NULL, 0, NULL, eq_sum1, 1, NULL, x, 60, 1e-9, &kkt) == 0;
    ok = ok && fabs(x[0] - 0.5) < 1e-6 && fabs(x[1] - 0.5) < 1e-6;
    {
        char d[80];
        snprintf(d, sizeof d, "x=(%.6f,%.6f) kkt=%.2e want (0.5,0.5)", x[0], x[1], kkt);
        test_record(ok, "sqp", "equality_qp_kkt", d);
        return ok;
    }
}

static int t_sqp_inequality(void)
{
    mlab_objective obj;
    double x[1] = {0.0}, kkt = 0;
    int ok;
    obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
    ok = mlab_sqp_min(&obj, ineq_x_le_1, 1, NULL, NULL, 0, NULL, x, 60, 1e-8, &kkt) == 0;
    ok = ok && fabs(x[0] - 1.0) < 1e-5;
    {
        char d[80];
        snprintf(d, sizeof d, "x=%.6f kkt=%.2e want ≈1", x[0], kkt);
        test_record(ok, "sqp", "inequality_active_set", d);
        return ok;
    }
}

/* ---- results 汇总 ---- */

static void write_csvs(void)
{
    FILE *fp = fopen("results/barrier_sqp.csv", "w");
    if (fp) {
        mlab_objective obj;
        double x[1] = {0.5}, kkt = 0;
        obj.f = f_shift; obj.grad = g_shift; obj.hess = NULL; obj.dim = 1; obj.ctx = NULL;
        mlab_log_barrier(&obj, ineq_x_le_1, 1, NULL, 1.0, x, 30, 1e-7, &kkt);
        fprintf(fp, "method,x,kkt\n");
        fprintf(fp, "log_barrier,%.8f,%.2e\n", x[0], kkt);
        x[0] = 0.0;
        kkt = 0;
        mlab_sqp_min(&obj, ineq_x_le_1, 1, NULL, NULL, 0, NULL, x, 60, 1e-8, &kkt);
        fprintf(fp, "sqp,%.8f,%.2e\n", x[0], kkt);
        fclose(fp);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"penalty", "large_mu_near_active_bound", "大罚系数逼到约束边界（真 KKT<1e-6）", t_penalty_pushes_to_boundary},
        {"aug_lag", "al_needs_1pct_of_penalty_mu", "AL 达同精度所需 σ ≤ 纯罚 μ 的 1%（L287）", t_penalty_vs_al_mu_scan},
        {"aug_lag", "equality_constraint_multiplier", "等式约束 AL 乘子恢复", t_auglag_equality_constraint},
        {"proj_grad", "box_clamp_unconstrained_outside", "无约束解在盒外时投影夹紧", t_proj_grad_box_clamp},
        {"proj_grad", "agrees_active_set_100_random", "100 随机箱式 QP 与有效集枚举一致（L288）", t_proj_grad_vs_active_set_random},
        {"box_qp", "active_set_hits_upper_bounds", "有效集法箱式 QP", t_box_qp_active_set},
        {"box_qp", "interior_solution", "盒内最优", t_box_qp_interior},
        {"interior", "log_barrier_converges", "对数障碍内点法（L277）", t_log_barrier_converges},
        {"sqp", "equality_qp_kkt", "最小 SQP 等式 QP（L279）", t_sqp_equality_qp},
        {"sqp", "inequality_active_set", "最小 SQP 不等式有效集", t_sqp_inequality},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("B5-constrained", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
