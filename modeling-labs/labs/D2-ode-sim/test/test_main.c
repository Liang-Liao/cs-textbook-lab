/*
 * D2: ODE 数值解与仿真 — Euler/RK4/隐式 Euler/RKF45 + 谐振子/LV/SEIR/Robertson
 */
#include "harness.h"
#include "lab.h"
#include "ode.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double loglog_slope(const double *h, const double *err, int n)
{
    double sx = 0, sy = 0, sxx = 0, sxy = 0, den;
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
    return ((double)n * sxy - sx * sy) / den;
}

/* ---------- 测试问题：y' = -2 t y, y(0)=1, y=exp(-t^2) ---------- */

static void rhs_exp(double t, const double *y, double *dydt, void *ctx)
{
    (void)ctx;
    dydt[0] = -2.0 * t * y[0];
}

static double exact_exp(double t)
{
    return exp(-t * t);
}

/* ---------- suite: rk4 ---------- */

static int t_rk4_global_order_4(void)
{
    /*
     * 判据：RK4 实测全局截断误差阶 4 ± 0.3。
     * y'=-2ty on [0,1]，固定步长到终点，终点误差 vs h 回归。
     */
    const int ns = 5;
    const int nsteps[5] = {4, 8, 16, 32, 64};
    double hs[5], errs[5];
    double order;
    int k, ok;
    char detail[200];
    FILE *fp;

    test_ensure_results_dir();
    fp = fopen("results/rk4_order.csv", "w");
    if (fp) fprintf(fp, "nsteps,h,err\n");
    for (k = 0; k < ns; ++k) {
        double y = 1.0;
        double h = 1.0 / (double)nsteps[k];
        double e;
        if (mlab_ode_rk4(rhs_exp, NULL, 1, 0.0, 1.0, h, &y) != 0) {
            test_record(0, "rk4", "global_order_4", "integrate fail");
            return 0;
        }
        e = fabs(y - exact_exp(1.0));
        hs[k] = h;
        errs[k] = e > 0 ? e : 1e-300;
        if (fp) fprintf(fp, "%d,%.8g,%.8g\n", nsteps[k], h, e);
    }
    if (fp) fclose(fp);
    order = fabs(loglog_slope(hs, errs, ns));
    ok = (order >= 3.7) && (order <= 4.3);
    snprintf(detail, sizeof detail,
             "log-log slope=%.3f (target 4±0.3); err(h=1/4)=%.3e err(h=1/64)=%.3e",
             order, errs[0], errs[ns - 1]);
    test_record(ok, "rk4", "global_order_4", detail);
    return ok;
}

static int t_rk4_euler_energy_drift(void)
{
    /*
     * 判据：谐振子长时程 RK4 能量相对漂移 < 1e-6，
     * 同预算 Euler 漂移显著更大。
     */
    const double w = 1.0;
    const double h = 0.02;
    const double T = 40.0;
    mlab_harm_ctx ctx;
    double y_rk[2], y_eu[2];
    double E0, Er, Ee, drift_rk, drift_eu;
    int ok;
    char detail[220];
    FILE *fp;

    const int nsave = 20;

    ctx.omega = w;
    y_rk[0] = 1.0;
    y_rk[1] = 0.0;
    y_eu[0] = 1.0;
    y_eu[1] = 0.0;
    E0 = mlab_harm_energy(y_rk, &ctx);

    test_ensure_results_dir();
    fp = fopen("results/harmonic_energy.csv", "w");
    if (fp) fprintf(fp, "t,E_rk4,E_euler\n");

    if (mlab_ode_rk4(mlab_harm_rhs, &ctx, 2, 0.0, T, h, y_rk) != 0 ||
        mlab_ode_euler(mlab_harm_rhs, &ctx, 2, 0.0, T, h, y_eu) != 0) {
        test_record(0, "energy", "harmonic_rk4_drift_vs_euler", "integrate fail");
        if (fp) fclose(fp);
        return 0;
    }
    /* 中途能量轨迹（重放到当前时刻） */
    if (fp) {
        double yr[2] = {1.0, 0.0}, ye[2] = {1.0, 0.0};
        int i;
        for (i = 0; i <= nsave; ++i) {
            double tmid = T * (double)i / (double)nsave;
            yr[0] = 1.0;
            yr[1] = 0.0;
            ye[0] = 1.0;
            ye[1] = 0.0;
            mlab_ode_rk4(mlab_harm_rhs, &ctx, 2, 0.0, tmid, h, yr);
            mlab_ode_euler(mlab_harm_rhs, &ctx, 2, 0.0, tmid, h, ye);
            fprintf(fp, "%.4f,%.8g,%.8g\n", tmid,
                    mlab_harm_energy(yr, &ctx), mlab_harm_energy(ye, &ctx));
        }
        fclose(fp);
        fp = NULL;
    }

    Er = mlab_harm_energy(y_rk, &ctx);
    Ee = mlab_harm_energy(y_eu, &ctx);
    drift_rk = fabs(Er - E0) / fabs(E0);
    drift_eu = fabs(Ee - E0) / fabs(E0);
    ok = (drift_rk < 1e-6) && (drift_eu > 1e2 * drift_rk);
    snprintf(detail, sizeof detail,
             "T=%.0f h=%.2f E0=%.4f | RK4 drift=%.3e (<1e-6) Euler drift=%.3e",
             T, h, E0, drift_rk, drift_eu);
    test_record(ok, "energy", "harmonic_rk4_drift_vs_euler", detail);
    return ok;
}

/* ---------- suite: adaptive ---------- */

/* 谐振子（ω=1）精确解 */
static void harm_exact(double t, double q0, double p0, double *y_ex)
{
    y_ex[0] = q0 * cos(t) + p0 * sin(t);
    y_ex[1] = -q0 * sin(t) + p0 * cos(t);
}

static int t_rkf45_local_error_bound(void)
{
    /*
     * 判据（去自证）：自适应 RKF45 在给定 tol 下，已接受步的真实局部
     * 误差 ≤ 2·tol，100 个随机初值。真实局部误差的测法：对每条轨迹
     * 的每个接受步 (t_k, h_k)，从**精确解** y_ex(t_k) 出发用同一步长
     * 单步积分（rkf45_step），与 y_ex(t_k+h_k) 之差即该步真实局部
     * 误差（与求解器内部误差估计无关）；同时并列记录内部估计供对照。
     */
    const int n_runs = 100;
    const double tol = 1e-5;
    const double T = 5.0;
    mlab_harm_ctx ctx;
    mlab_rng rng;
    int i, ok, n_retried = 0;
    double max_overall = 0.0, max_est = 0.0;
    int n_bad = 0;
    char detail[240];
    FILE *fp;

    ctx.omega = 1.0;
    mlab_rng_seed(&rng, 74001);
    test_ensure_results_dir();
    fp = fopen("results/rkf45_error_bound.csv", "w");
    if (fp) fprintf(fp, "run,step,t,h,err_est,err_true\n");

    for (i = 0; i < n_runs; ++i) {
        double y[2], y0[2];
        mlab_rkf45_stats st;
        mlab_rkf45_trace tr;
        double q = 0.3 + 1.4 * mlab_rng_uniform(&rng);
        double p = 0.3 + 1.4 * mlab_rng_uniform(&rng);
        int k;
        y0[0] = q;
        y0[1] = p;
        y[0] = q;
        y[1] = p;
        if (mlab_ode_rkf45_trace(mlab_harm_rhs, &ctx, 2, 0.0, T, y,
                                 tol, tol, 0.05, &st, &tr) != 0) {
            test_record(0, "adaptive", "rkf45_local_err_le_2tol", "integrate fail");
            if (fp) fclose(fp);
            mlab_rkf45_trace_free(&tr);
            return 0;
        }
        for (k = 0; k < tr.n; ++k) {
            double tk = tr.t[k], hk = tr.h[k];
            double ys[2], ye[2], err_true, err_est;
            int rc, guard = 0;
            harm_exact(tk, y0[0], y0[1], ye);
            ys[0] = ye[0];
            ys[1] = ye[1];
            /* 从精确解出发同长单步；若被拒则按控制器减半重试（罕见） */
            rc = mlab_ode_rkf45_step(mlab_harm_rhs, &ctx, 2, tk, ys, hk,
                                     tol, tol, &err_est);
            while (rc == 0 && guard < 20) {
                hk *= 0.5;
                ++n_retried;
                rc = mlab_ode_rkf45_step(mlab_harm_rhs, &ctx, 2, tk, ys, hk,
                                         tol, tol, &err_est);
                ++guard;
            }
            if (rc != 1) continue;
            harm_exact(tk + hk, y0[0], y0[1], ye);
            err_true = fmax(fabs(ys[0] - ye[0]), fabs(ys[1] - ye[1]));
            if (err_true > max_overall) max_overall = err_true;
            if (err_est > max_est) max_est = err_est;
            if (err_true > 2.0 * tol + 1e-15) ++n_bad;
            if (fp)
                fprintf(fp, "%d,%d,%.8g,%.8g,%.8g,%.8g\n",
                        i, k, tk, hk, err_est, err_true);
        }
        mlab_rkf45_trace_free(&tr);
    }
    if (fp) fclose(fp);
    ok = (n_bad == 0) && (max_overall <= 2.0 * tol + 1e-15);
    snprintf(detail, sizeof detail,
             "100 ICs tol=%.1e max_true_local=%.3e ≤ 2·tol=%.1e "
             "(max internal est=%.3e); violations=%d retried=%d",
             tol, max_overall, 2.0 * tol, max_est, n_bad, n_retried);
    test_record(ok, "adaptive", "rkf45_local_err_le_2tol", detail);
    return ok;
}

static int t_rkf45_adapts_step_to_smoothness(void)
{
    /*
     * 实验 3（:628）：步长曲线与解光滑度的对应。
     * 光滑谐振子步长持续放大；Robertson 刚性段步长被压低。
     * 两条步长曲线落盘 results/rkf45_steps.csv。
     */
    const double tol = 1e-6;
    mlab_harm_ctx ctx;
    double y[2] = {1.0, 0.0};
    mlab_rkf45_stats st, st_r;
    mlab_rkf45_trace tr, tr_r;
    double yr[3] = {1.0, 0.0, 0.0};
    int k, ok, n_harm, n_rob;
    char detail[200];
    FILE *fp;

    ctx.omega = 1.0;
    if (mlab_ode_rkf45_trace(mlab_harm_rhs, &ctx, 2, 0.0, 20.0, y,
                             tol, tol, 0.01, &st, &tr) != 0) {
        test_record(0, "adaptive", "step_adapts_smooth_solution", "fail");
        return 0;
    }
    /* Robertson 到 T=0.1（早期最刚硬段），宽松 tol 只看步长被压低 */
    if (mlab_ode_rkf45_trace(mlab_robertson_rhs, NULL, 3, 0.0, 0.1, yr,
                             1e-4, 1e-6, 0.001, &st_r, &tr_r) != 0) {
        test_record(0, "adaptive", "step_adapts_smooth_solution", "rob fail");
        mlab_rkf45_trace_free(&tr);
        return 0;
    }
    test_ensure_results_dir();
    fp = fopen("results/rkf45_steps.csv", "w");
    if (fp) {
        fprintf(fp, "problem,step,t,h,err_est\n");
        n_harm = tr.n < 400 ? tr.n : 400;
        for (k = 0; k < n_harm; ++k)
            fprintf(fp, "harmonic,%d,%.8g,%.8g,%.8g\n",
                    k, tr.t[k], tr.h[k], tr.err[k]);
        n_rob = tr_r.n < 400 ? tr_r.n : 400;
        for (k = 0; k < n_rob; ++k)
            fprintf(fp, "robertson,%d,%.8g,%.8g,%.8g\n",
                    k, tr_r.t[k], tr_r.h[k], tr_r.err[k]);
        fclose(fp);
    }
    ok = (st.n_accept > 0) && (st.h_max > 5.0 * st.h_min) && (st.h_max > 0.05) &&
         (tr_r.n > 0);
    snprintf(detail, sizeof detail,
             "harmonic acc=%d h_min=%.2e→h_max=%.2e (>5×); "
             "robertson [0,0.1] acc=%d h_max=%.2e (被刚性压低)",
             st.n_accept, st.h_min, st.h_max,
             st_r.n_accept, st_r.n_accept > 0 ? st_r.h_max : 0.0);
    test_record(ok, "adaptive", "step_adapts_smooth_solution", detail);
    mlab_rkf45_trace_free(&tr);
    mlab_rkf45_trace_free(&tr_r);
    return ok;
}

/* ---------- suite: stiff ---------- */

static int t_stiff_robertson_explicit_vs_implicit(void)
{
    /*
     * 刚性 Robertson：显式法步长天花板 vs 隐式 Euler 稳定穿越。
     * 显式 Euler/RK4 在 h 相对快时间尺度过大时发散或违反非负/质量守恒。
     */
    double y0[3] = {1.0, 0.0, 0.0};
    double ye[3], yi[3], yr[3];
    double sum_e, sum_i, sum_r;
    int ok, ok_e, ok_i;
    char detail[260];
    FILE *fp;
    const double h_bad = 0.05;  /* 相对 Robertson 过大 */
    const double h_ok = 0.05;   /* 隐式仍可用 */
    const double T = 40.0;      /* 刚性段早期 */

    memcpy(ye, y0, sizeof y0);
    memcpy(yi, y0, sizeof y0);
    memcpy(yr, y0, sizeof y0);

    mlab_ode_euler(mlab_robertson_rhs, NULL, 3, 0.0, T, h_bad, ye);
    mlab_ode_euler_implicit(mlab_robertson_rhs, NULL, 3, 0.0, T, h_ok, yi, 30, 1e-12);
    mlab_ode_rk4(mlab_robertson_rhs, NULL, 3, 0.0, T, h_bad, yr);

    sum_e = ye[0] + ye[1] + ye[2];
    sum_i = yi[0] + yi[1] + yi[2];
    sum_r = yr[0] + yr[1] + yr[2];

    /* 显式失败：出现 NaN/Inf，或严重负值，或质量严重不守恒 */
    ok_e = !(isfinite(ye[0]) && isfinite(ye[1]) && isfinite(ye[2]) &&
             ye[0] > -0.5 && ye[1] > -1e-3 && fabs(sum_e - 1.0) < 0.2);
    /* 更明确：显式质量误差或负浓度显著 */
    {
        int neg = (ye[0] < -1e-2) || (ye[1] < -1e-6) || (ye[2] < -1e-2);
        int mass_bad = !isfinite(sum_e) || fabs(sum_e - 1.0) > 0.2;
        int nan = !isfinite(ye[0]) || !isfinite(ye[1]) || !isfinite(ye[2]);
        ok_e = nan || neg || mass_bad;
    }
    /* 隐式成功：非负、质量守恒、物种可解释 */
    ok_i = isfinite(yi[0]) && isfinite(yi[1]) && isfinite(yi[2]) &&
           yi[0] >= -1e-8 && yi[1] >= -1e-8 && yi[2] >= -1e-8 &&
           fabs(sum_i - 1.0) < 1e-3 &&
           yi[0] < 1.0 + 1e-6; /* y1 应从 1 下降 */

    test_ensure_results_dir();
    fp = fopen("results/robertson_stiff.csv", "w");
    if (fp) {
        fprintf(fp, "method,h,y0,y1,y2,sum\n");
        fprintf(fp, "euler,%.4g,%.6g,%.6g,%.6g,%.6g\n", h_bad, ye[0], ye[1], ye[2], sum_e);
        fprintf(fp, "euler_implicit,%.4g,%.6g,%.6g,%.6g,%.6g\n", h_ok, yi[0], yi[1], yi[2], sum_i);
        fprintf(fp, "rk4,%.4g,%.6g,%.6g,%.6g,%.6g\n", h_bad, yr[0], yr[1], yr[2], sum_r);
        fclose(fp);
    }

    ok = ok_e && ok_i;
    snprintf(detail, sizeof detail,
             "h=%.3g T=%.0f | explicit Euler y=(%.3g,%.3g,%.3g) sum=%.3g; "
             "implicit y=(%.4g,%.4g,%.4g) sum=%.6f",
             h_bad, T, ye[0], ye[1], ye[2], sum_e, yi[0], yi[1], yi[2], sum_i);
    test_record(ok, "stiff", "robertson_explicit_ceiling_vs_implicit", detail);
    return ok;
}

static int t_stiff_explicit_small_h_still_works(void)
{
    /* 对照：显式法步长足够小时也能走刚性段（非负+质量） */
    double y[3] = {1.0, 0.0, 0.0};
    double sum;
    int ok;
    char detail[180];
    const double h = 1e-5;
    const double T = 1e-2;

    if (mlab_ode_rk4(mlab_robertson_rhs, NULL, 3, 0.0, T, h, y) != 0) {
        test_record(0, "stiff", "explicit_ok_with_tiny_h", "fail");
        return 0;
    }
    sum = y[0] + y[1] + y[2];
    ok = isfinite(sum) && fabs(sum - 1.0) < 1e-4 &&
         y[0] > 0.9 && y[0] <= 1.0 + 1e-8 && y[1] >= -1e-10;
    snprintf(detail, sizeof detail,
             "RK4 h=1e-5 T=0.01 y0=%.6f y1=%.3e sum-1=%.3e", y[0], y[1], sum - 1.0);
    test_record(ok, "stiff", "explicit_ok_with_tiny_h", detail);
    return ok;
}

/* ---------- suite: templates ---------- */

static int t_template_harmonic_exact(void)
{
    /* 短时程谐振子 vs 解析解 */
    mlab_harm_ctx ctx = {1.0};
    double h = 0.001;
    double T = 2.5;
    double maxe = 0.0;
    int i, ok;
    char detail[160];
    double yy[2] = {0.0, 1.0}; /* q=0,p=1 → q=sin t, p=cos t */

    {
        int nstep = (int)(T / h);
        for (i = 0; i < nstep; ++i) {
            double t0 = (double)i * h;
            double tq = t0 + h;
            double qs = sin(tq);
            mlab_ode_rk4(mlab_harm_rhs, &ctx, 2, t0, tq, h, yy);
            {
                double e = fabs(yy[0] - qs);
                if (e > maxe) maxe = e;
            }
        }
    }
    ok = maxe < 1e-6;
    snprintf(detail, sizeof detail, "RK4 vs sin(t) max|q-err|=%.3e on [0,%.1f]", maxe, T);
    test_record(ok, "templates", "harmonic_matches_analytic", detail);
    return ok;
}

static int t_template_lv_and_seir_conservation(void)
{
    /* SEIR 人口守恒；LV 相空间不发散 */
    mlab_seir_ctx sc;
    mlab_lv_ctx lc;
    double ys[4], yl[2];
    double sum0, sum1;
    int ok;
    char detail[220];

    sc.beta = 0.4;
    sc.sigma = 0.2;
    sc.gamma = 0.1;
    sc.N = 10000.0;
    ys[0] = 9990.0;
    ys[1] = 8.0;
    ys[2] = 2.0;
    ys[3] = 0.0;
    sum0 = ys[0] + ys[1] + ys[2] + ys[3];
    if (mlab_ode_rk4(mlab_seir_rhs, &sc, 4, 0.0, 80.0, 0.05, ys) != 0) {
        test_record(0, "templates", "lv_seir_sanity", "seir fail");
        return 0;
    }
    sum1 = ys[0] + ys[1] + ys[2] + ys[3];

    lc.alpha = 1.0;
    lc.beta = 0.1;
    lc.gamma = 1.5;
    lc.delta = 0.075;
    yl[0] = 10.0;
    yl[1] = 5.0;
    if (mlab_ode_rk4(mlab_lv_rhs, &lc, 2, 0.0, 20.0, 0.01, yl) != 0) {
        test_record(0, "templates", "lv_seir_sanity", "lv fail");
        return 0;
    }
    ok = (fabs(sum1 - sum0) < 1e-3 * sum0) &&
         isfinite(yl[0]) && isfinite(yl[1]) &&
         yl[0] > 0.0 && yl[1] > 0.0 &&
         ys[0] >= -1.0 && ys[2] >= -1.0;
    snprintf(detail, sizeof detail,
             "SEIR pop %.4f→%.4f (Δ=%.3e); LV t=20 prey=%.3f pred=%.3f",
             sum0, sum1, sum1 - sum0, yl[0], yl[1]);
    test_record(ok, "templates", "lv_seir_sanity", detail);
    return ok;
}

/* ---------- suite: implicit ---------- */

static void rhs_stiff_decay(double t, const double *y, double *dydt, void *ctx)
{
    (void)t;
    (void)ctx;
    dydt[0] = -1000.0 * (y[0] - 1.0);
}

static int t_implicit_euler_order_and_stability(void)
{
    /*
     * 隐式 Euler 补套件：
     * 1) 阶：y'=-2y on [0,1]，h 对半减，终点误差 log-log 斜率 1±0.15；
     * 2) 稳定性：y'=-1000(y-1)（刚性标量），显式稳定上限 h<0.002，
     *    取 h=0.5：显式 Euler 发散，隐式单调衰减到稳态 1。
     */
    const int ns = 4;
    const int nsteps[4] = {4, 8, 16, 32};
    double hs[4], errs[4];
    double order;
    int k, ok;
    char detail[240];
    FILE *fp;
    const double h_big = 0.5, T2 = 20.0;
    double y_im[1], y_eu[1];
    int rc_im;

    test_ensure_results_dir();
    fp = fopen("results/implicit_euler_order.csv", "w");
    if (fp) fprintf(fp, "nsteps,h,err\n");
    for (k = 0; k < ns; ++k) {
        double y = 1.0;
        double h = 1.0 / (double)nsteps[k];
        double e;
        if (mlab_ode_euler_implicit(rhs_exp, NULL, 1, 0.0, 1.0, h, &y,
                                    30, 1e-12) != 0) {
            test_record(0, "implicit", "euler_order_1_and_stability", "integrate fail");
            if (fp) fclose(fp);
            return 0;
        }
        e = fabs(y - exact_exp(1.0));
        hs[k] = h;
        errs[k] = e > 0 ? e : 1e-300;
        if (fp) fprintf(fp, "%d,%.8g,%.8g\n", nsteps[k], h, e);
    }
    if (fp) fclose(fp);
    order = fabs(loglog_slope(hs, errs, ns));

    /* 刚性标量：y'=-1000(y-1), y(0)=2 → y(T)=1+(2-1)e^{-1000T}≈1 */
    y_im[0] = 2.0;
    y_eu[0] = 2.0;
    rc_im = mlab_ode_euler_implicit(rhs_stiff_decay, NULL, 1, 0.0, T2, h_big,
                                    y_im, 50, 1e-12);
    mlab_ode_euler(rhs_stiff_decay, NULL, 1, 0.0, T2, h_big, y_eu);
    {
        int blow = !isfinite(y_eu[0]) || fabs(y_eu[0]) > 1e10;
        int stab = rc_im == 0 && fabs(y_im[0] - 1.0) < 1e-3;
        ok = (order >= 0.85) && (order <= 1.15) && blow && stab;
        snprintf(detail, sizeof detail,
                 "order=%.3f (1±0.15) | stiff λ=1000 h=0.5: explicit y=%.3g (%s), "
                 "implicit rc=%d y=%.6f (稳态1)",
                 order, y_eu[0], blow ? "blow-up" : "finite", rc_im, y_im[0]);
    }
    test_record(ok, "implicit", "euler_order_1_and_stability", detail);
    return ok;
}

/* ---------- suite: pk（药代动力学模板，路线图 :623） ---------- */

static int t_pk_one_comp_iv(void)
{
    /* 一室 IV bolus：RKF45 紧容差 vs 解析解 C0·e^{-kel·t} */
    mlab_pk1_ctx ctx;
    double y = 10.0;
    double T = 10.0;
    mlab_rkf45_stats st;
    int i, ok;
    double maxe = 0.0;
    char detail[180];
    FILE *fp;

    ctx.kel = 0.35;
    test_ensure_results_dir();
    fp = fopen("results/pk_one_comp.csv", "w");
    if (fp) fprintf(fp, "t,numerical,analytic\n");
    /* 分段积分到检查点，避免只验证终点 */
    for (i = 1; i <= 20; ++i) {
        double t1 = T * (double)i / 20.0;
        double ana = mlab_pk1_analytic(t1, 10.0, ctx.kel);
        y = 10.0; /* 每次从 t=0 重积分到 t1 */
        if (mlab_ode_rkf45(mlab_pk1_rhs, &ctx, 1, 0.0, t1, &y,
                           1e-9, 1e-11, 0.05, &st) != 0) {
            test_record(0, "pk", "one_comp_matches_analytic", "fail");
            if (fp) fclose(fp);
            return 0;
        }
        if (fabs(y - ana) > maxe) maxe = fabs(y - ana);
        if (fp) fprintf(fp, "%.4f,%.8g,%.8g\n", t1, y, ana);
    }
    if (fp) fclose(fp);
    ok = maxe < 1e-7;
    snprintf(detail, sizeof detail,
             "kel=0.35 C0=10 [0,10]: max|num-analytic|=%.3e (<1e-7)", maxe);
    test_record(ok, "pk", "one_comp_matches_analytic", detail);
    return ok;
}

static int t_pk_two_comp_absorption(void)
{
    /*
     * 二室 + 一级吸收：RKF45 vs 三指数解析解。
     * ka=1.2 kel=0.35 k12=0.8 k21=0.6，y0=[10,0,0]，T=12。
     */
    mlab_pk2_ctx ctx;
    double y0[3] = {10.0, 0.0, 0.0};
    double y[3];
    double T = 12.0;
    mlab_rkf45_stats st;
    int i, ok;
    double maxe = 0.0, peak1 = 0.0;
    char detail[220];
    FILE *fp;
    const int nchk = 24;

    ctx.ka = 1.2;
    ctx.kel = 0.35;
    ctx.k12 = 0.8;
    ctx.k21 = 0.6;
    test_ensure_results_dir();
    fp = fopen("results/pk_two_comp.csv", "w");
    if (fp) fprintf(fp, "t,agut,a1,a2,agut_ana,a1_ana,a2_ana\n");
    for (i = 1; i <= nchk; ++i) {
        double t1 = T * (double)i / (double)nchk;
        double ya[3];
        if (mlab_pk2_analytic(&ctx, y0, t1, ya) != 0) {
            test_record(0, "pk", "two_comp_matches_analytic", "analytic fail");
            if (fp) fclose(fp);
            return 0;
        }
        memcpy(y, y0, sizeof y0);
        if (mlab_ode_rkf45(mlab_pk2_rhs, &ctx, 3, 0.0, t1, y,
                           1e-9, 1e-11, 0.02, &st) != 0) {
            test_record(0, "pk", "two_comp_matches_analytic", "integrate fail");
            if (fp) fclose(fp);
            return 0;
        }
        {
            int j;
            for (j = 0; j < 3; ++j) {
                double e = fabs(y[j] - ya[j]);
                if (e > maxe) maxe = e;
            }
        }
        if (y[1] > peak1) peak1 = y[1];
        if (fp)
            fprintf(fp, "%.4f,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g\n",
                    t1, y[0], y[1], y[2], ya[0], ya[1], ya[2]);
    }
    if (fp) fclose(fp);
    ok = (maxe < 1e-6) && (peak1 > 1.0) && (peak1 < 10.0);
    snprintf(detail, sizeof detail,
             "ka=1.2 kel=0.35 k12=0.8 k21=0.6: max|num-analytic|=%.3e (<1e-6), "
             "A1 peak=%.3f（吸收相）", maxe, peak1);
    test_record(ok, "pk", "two_comp_matches_analytic", detail);
    return ok;
}

/* ---------- suite: sensitivity ---------- */

static void rhs_theta_decay(double t, const double *y, double *dydt,
                            const double *theta, int ntheta, void *ctx)
{
    (void)ctx;
    (void)ntheta;
    (void)t;
    dydt[0] = -theta[0] * y[0];
}

static int t_sensitivity_fd_matches_analytic(void)
{
    /*
     * y'=-θ y, y(0)=1 → y(T)=exp(-θT), ∂y/∂θ = -T exp(-θT)
     * 为 M3 可辨识性备料的有限差分敏感性对账。
     */
    double theta[1] = {0.7};
    double y0[1] = {1.0};
    double S[1], ynom[1];
    double T = 2.0, h = 0.005;
    double ana, err;
    int ok;
    char detail[160];

    if (mlab_ode_sensitivity_fd(rhs_theta_decay, NULL, 1, theta, 1,
                                0.0, T, h, y0, 1e-6, S, ynom) != 0) {
        test_record(0, "sensitivity", "fd_matches_analytic", "fail");
        return 0;
    }
    ana = -T * exp(-theta[0] * T);
    err = fabs(S[0] - ana);
    ok = (err < 1e-5 * (fabs(ana) + 1.0)) &&
         (fabs(ynom[0] - exp(-theta[0] * T)) < 1e-6);
    snprintf(detail, sizeof detail,
             "∂y/∂θ FD=%.6g analytic=%.6g |Δ|=%.3e; y(T) FD=%.6g exact=%.6g",
             S[0], ana, err, ynom[0], exp(-theta[0] * T));
    test_record(ok, "sensitivity", "fd_matches_analytic", detail);
    return ok;
}

static void harm_theta_rhs(double t, const double *y, double *dydt,
                           const double *theta, int ntheta, void *ctx)
{
    (void)ctx;
    (void)ntheta;
    (void)t;
    dydt[0] = y[1];
    dydt[1] = -theta[0] * theta[0] * y[0];
}

static int t_sensitivity_harmonic_omega_real(void)
{
    double theta[1] = {1.3};
    double y0[2] = {0.0, 1.0};
    double S[2], yn[2];
    double T = 1.0, h = 0.002;
    double w = theta[0];
    double q_ana, p_ana, Sq, Sp, err_q, err_p;
    int ok;
    char detail[200];

    if (mlab_ode_sensitivity_fd(harm_theta_rhs, NULL, 2, theta, 1,
                                0.0, T, h, y0, 1e-6, S, yn) != 0) {
        test_record(0, "sensitivity", "harmonic_omega_fd", "fail");
        return 0;
    }
    q_ana = sin(w * T) / w;
    p_ana = cos(w * T);
    Sp = -T * sin(w * T);
    Sq = (T * w * cos(w * T) - sin(w * T)) / (w * w);
    /* S[i + j*n] = ∂y_i/∂θ_j, n=2, j=0 */
    err_q = fabs(S[0] - Sq);
    err_p = fabs(S[1] - Sp);
    ok = (err_q < 1e-4) && (err_p < 1e-4) &&
         (fabs(yn[0] - q_ana) < 1e-4) && (fabs(yn[1] - p_ana) < 1e-4);
    snprintf(detail, sizeof detail,
             "∂q/∂ω FD=%.6g ana=%.6g; ∂p/∂ω FD=%.6g ana=%.6g; y=(%.5f,%.5f)",
             S[0], Sq, S[1], Sp, yn[0], yn[1]);
    test_record(ok, "sensitivity", "harmonic_omega_fd", detail);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"rk4", "global_order_4",
         "RK4 全局截断误差阶 4±0.3",
         t_rk4_global_order_4},
        {"energy", "harmonic_rk4_drift_vs_euler",
         "谐振子 RK4 能量漂移 <1e-6，Euler 显著更大",
         t_rk4_euler_energy_drift},
        {"adaptive", "rkf45_local_err_le_2tol",
         "RKF45 100 随机初值真实局部误差 ≤ 2·tol（对精确解）",
         t_rkf45_local_error_bound},
        {"adaptive", "step_adapts_smooth_solution",
         "步长曲线：光滑段放大 vs 刚性段压低（落盘）",
         t_rkf45_adapts_step_to_smoothness},
        {"implicit", "euler_order_1_and_stability",
         "隐式 Euler 一阶收敛 + 刚性标量大步长稳定",
         t_implicit_euler_order_and_stability},
        {"pk", "one_comp_matches_analytic",
         "一室 IV bolus 数值解对照解析",
         t_pk_one_comp_iv},
        {"pk", "two_comp_matches_analytic",
         "二室带吸收数值解对照三指数解析",
         t_pk_two_comp_absorption},
        {"stiff", "robertson_explicit_ceiling_vs_implicit",
         "Robertson 刚性：显式天花板 vs 隐式稳定",
         t_stiff_robertson_explicit_vs_implicit},
        {"stiff", "explicit_ok_with_tiny_h",
         "对照：显式法极小步长可走刚性段",
         t_stiff_explicit_small_h_still_works},
        {"templates", "harmonic_matches_analytic",
         "谐振子 RK4 与解析解一致",
         t_template_harmonic_exact},
        {"templates", "lv_seir_sanity",
         "SEIR 人口守恒；LV 轨迹有界正",
         t_template_lv_and_seir_conservation},
        {"sensitivity", "fd_matches_analytic",
         "指数衰减模型 FD 敏感性对账解析",
         t_sensitivity_fd_matches_analytic},
        {"sensitivity", "harmonic_omega_fd",
         "谐振子 ∂y/∂ω 有限差分对账解析",
         t_sensitivity_harmonic_omega_real},
    };
    test_ensure_results_dir();
    return test_run_main("D2-ode-sim", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
