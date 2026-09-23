#ifndef MLAB_ODE_H
#define MLAB_ODE_H

/* D2 ODE 数值解与仿真模板 */

/* y' = f(t, y, ctx)，y 长度 n */
typedef void (*mlab_ode_rhs)(double t, const double *y, double *dydt, void *ctx);

/* ---------- 固定步长积分 ---------- */
int mlab_ode_euler(mlab_ode_rhs f, void *ctx, int n,
                   double t0, double t1, double h, double *y);

int mlab_ode_rk4(mlab_ode_rhs f, void *ctx, int n,
                 double t0, double t1, double h, double *y);

/* 隐式 Euler：单步 Newton，每步求解 y - h f(t+h,y) = y_old
 * 返回 0 成功；-1 参数/分配错误；-2 线性求解失败；-3 Newton 不收敛 */
int mlab_ode_euler_implicit(mlab_ode_rhs f, void *ctx, int n,
                            double t0, double t1, double h, double *y,
                            int max_newton, double newton_tol);

/* ---------- 自适应 RKF45（嵌入 4/5 阶） ---------- */
typedef struct {
    int n_accept;
    int n_reject;
    int n_fev;
    double h_min;
    double h_max;
    double max_err_est; /* 已接受步上的最大局部误差估计（未缩放 |y5-y4|） */
} mlab_rkf45_stats;

/* 每接受步记录（步长曲线 / 与精确解对照用）；n 条记录，调用方 free */
typedef struct {
    double *t;   /* 接受步起点 */
    double *h;   /* 接受步步长 */
    double *err; /* 未缩放 |y5-y4| 最大分量（该步的局部误差估计） */
    int n, cap;
} mlab_rkf45_trace;

void mlab_rkf45_trace_free(mlab_rkf45_trace *tr);

/*
 * 积分 y(t0)->t1，容差 tol（每步局部误差）。
 * y 为初值输入、终值输出。stats/trace 均可空。
 * 返回 0 成功，-1 参数错误，-2 步长下溢/失败。
 */
int mlab_ode_rkf45_trace(mlab_ode_rhs f, void *ctx, int n,
                         double t0, double t1, double *y,
                         double rtol, double atol,
                         double h_init, mlab_rkf45_stats *stats,
                         mlab_rkf45_trace *trace);

int mlab_ode_rkf45(mlab_ode_rhs f, void *ctx, int n,
                   double t0, double t1, double *y,
                   double rtol, double atol,
                   double h_init, mlab_rkf45_stats *stats);

/*
 * 单步 RKF45：固定步长 h 从 t 试一步（去自证测试用）。
 * 接受返回 1（y 更新为 5 阶解，err_est 输出未缩放 |y5-y4| 最大分量）；
 * 拒绝返回 0（y 不变）；-1 参数/分配错误。
 */
int mlab_ode_rkf45_step(mlab_ode_rhs f, void *ctx, int n,
                        double t, double *y, double h,
                        double rtol, double atol, double *err_est);

/* ---------- 仿真模板（ctx 可传参数指针） ---------- */

/* 谐振子：q'=p, p'=-ω² q；能量 E=0.5(p²+ω²q²) */
typedef struct { double omega; } mlab_harm_ctx;
void mlab_harm_rhs(double t, const double *y, double *dydt, void *ctx);
double mlab_harm_energy(const double *y, const mlab_harm_ctx *ctx);

/* Lotka-Volterra：x'=αx-βxy, y'=δxy-γy */
typedef struct { double alpha, beta, gamma, delta; } mlab_lv_ctx;
void mlab_lv_rhs(double t, const double *y, double *dydt, void *ctx);

/* SEIR：S,E,I,R 标准双线性感染 */
typedef struct {
    double beta;   /* 传染率 */
    double sigma;  /* E->I */
    double gamma;  /* 恢复率 */
    double N;      /* 总人口 */
} mlab_seir_ctx;
void mlab_seir_rhs(double t, const double *y, double *dydt, void *ctx);

/* Robertson 刚性：三物种化学反应 */
void mlab_robertson_rhs(double t, const double *y, double *dydt, void *ctx);

/* ---------- 药代动力学房室模板（路线图 :623 标准仿真模板） ---------- */

/* 一室 IV bolus：C' = -kel·C；解析解 C(t)=C0·exp(-kel·t) */
typedef struct { double kel; } mlab_pk1_ctx;
void mlab_pk1_rhs(double t, const double *y, double *dydt, void *ctx);
double mlab_pk1_analytic(double t, double C0, double kel);

/*
 * 二室 + 一级吸收（血管外给药）：y = [A_gut, A_1, A_2]（药量，V=1）
 *   A_g' = -ka·A_g
 *   A_1' = ka·A_g - (kel+k12)·A_1 + k21·A_2
 *   A_2' = k12·A_1 - k21·A_2
 * 解析解（三指数）：A_g=A_g0·e^{-ka t}；A_1=C_a e^{-ka t}+C_1 e^{λ1 t}+C_2 e^{λ2 t}，
 * 齐次系数由初值与 ODE 右端在 t=0 的约束解 2×2 线性系统；A_2 同型由
 * 房室间关系给出。ka 与特征值重合等退化情形返回 -1。
 */
typedef struct { double ka, kel, k12, k21; } mlab_pk2_ctx;
void mlab_pk2_rhs(double t, const double *y, double *dydt, void *ctx);
int mlab_pk2_analytic(const mlab_pk2_ctx *c, const double *y0, double t,
                      double *y_out);

/* ---------- 参数敏感性（有限差分） ---------- */
/*
 * 用中心差分估计 ∂y(t_end)/∂θ_j。
 * f/ctx 依赖参数数组 theta；rhs 需读 ctx 内 theta。
 * 敏感性长度 n * n_theta，行主序：S[i + j*n] = ∂y_i/∂θ_j
 */
typedef void (*mlab_ode_rhs_theta)(double t, const double *y, double *dydt,
                                   const double *theta, int ntheta, void *ctx);

int mlab_ode_sensitivity_fd(mlab_ode_rhs_theta f, void *ctx, int n,
                            const double *theta, int ntheta,
                            double t0, double t1, double h,
                            const double *y0, double eps_rel,
                            double *S_out, double *y_nominal);

#endif /* MLAB_ODE_H */
