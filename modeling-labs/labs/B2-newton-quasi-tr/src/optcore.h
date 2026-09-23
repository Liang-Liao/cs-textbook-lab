#ifndef MLAB_OPTCORE_H
#define MLAB_OPTCORE_H

#include "opt.h"

typedef struct {
    double f_final;
    double gnorm;
    int iters;
    int fevals;
    int gevals;
    int status; /* 0 converged, 1 maxiter, -1 fail */
    double *f_hist; /* optional, set via mlab_opt_run_init */
    int hist_len;
} mlab_opt_run;

/* Zero run and attach optional residual/function history buffer. */
void mlab_opt_run_init(mlab_opt_run *run, double *f_hist, int hist_len);

/* Steepest descent + Armijo. */
int mlab_opt_gd(const mlab_objective *obj, double *x, double tol,
                int max_iter, mlab_opt_run *run);

/* Newton with Hessian modification (add ridge if not PD) + Armijo. */
int mlab_opt_newton(const mlab_objective *obj, double *x, double tol,
                    int max_iter, mlab_opt_run *run);

/* BFGS quasi-Newton + Armijo. */
int mlab_opt_bfgs(const mlab_objective *obj, double *x, double tol,
                  int max_iter, mlab_opt_run *run);

/* Linear CG for SPD quadratic: A x = b (min 0.5 x^T A x - b^T x). */
int mlab_cg_solve(const double *A, int n, const double *b, double *x,
                  int max_iter, double tol, int *iters_out, double *res_out);

/* Dogleg trust-region on general objective. */
int mlab_opt_dogleg(const mlab_objective *obj, double *x, double tol,
                    int max_iter, double delta0, mlab_opt_run *run);

/* Nelder-Mead simplex. */
int mlab_opt_neldermead(const mlab_objective *obj, double *x, double tol,
                        int max_iter, mlab_opt_run *run);

/* L-BFGS（双循环递推，保留 mem 个 (s,y) 对，mem>=1）。 */
int mlab_opt_lbfgs(const mlab_objective *obj, double *x, double tol,
                   int max_iter, int mem, mlab_opt_run *run);

/* DFP 拟牛顿（H+ = H - HyHy^T/yHy + ss^T/sy）。 */
int mlab_opt_dfp(const mlab_objective *obj, double *x, double tol,
                 int max_iter, mlab_opt_run *run);

/* 坐标下降：每坐标在 ±bracket 内做黄金分割精确一维最小化（无梯度）。 */
int mlab_opt_coord_descent(const mlab_objective *obj, double *x, double tol,
                           int max_sweeps, double bracket, mlab_opt_run *run);

/* Hooke-Jeeves 模式搜索（探测移动 + 模式移动，步长减半，无梯度）。 */
int mlab_opt_hooke_jeeves(const mlab_objective *obj, double *x, double tol,
                          int max_iter, double step0, mlab_opt_run *run);

/*
 * DIRECT 全局搜索（分割超矩形；单维三分划分变体；无梯度）。
 * 在盒 [lb,ub] 内搜索，评估预算 max_evals，边长 < side_tol 视为收敛。
 * 返回 0 = side_tol 达成，1 = 预算用尽；x_best_out 由调用方分配 n 个。
 */
int mlab_direct(const mlab_objective *obj, const double *lb, const double *ub,
                int n, int max_evals, double side_tol,
                double *x_best_out, double *f_best_out, int *n_eval_out);

#endif /* MLAB_OPTCORE_H */
