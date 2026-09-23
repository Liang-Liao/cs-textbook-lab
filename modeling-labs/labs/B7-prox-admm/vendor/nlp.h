#ifndef MLAB_NLP_H
#define MLAB_NLP_H

#include "opt.h"

/* Outer penalty: min f(x) + mu * sum max(0, g_i(x))^2 for inequality g<=0.
   eq optional: mu * ||h(x)||^2. Returns projected/penalty iterate. */
typedef void (*mlab_ineq_fn)(const double *x, int n, double *g, int m_ineq, void *ctx);
typedef void (*mlab_eq_fn)(const double *x, int n, double *h, int m_eq, void *ctx);

int mlab_penalty_outer(const mlab_objective *obj,
                       mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                       mlab_eq_fn eq, int m_eq, void *eq_ctx,
                       double mu, double *x, int max_iter, double tol,
                       double *kkt_res_out);

int mlab_aug_lag(const mlab_objective *obj,
                 mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                 mlab_eq_fn eq, int m_eq, void *eq_ctx,
                 double sigma0, double *x, double *lam, double *nu,
                 int max_iter, double tol, double *kkt_res_out);

/* Projected gradient for box constraints lb<=x<=ub
   （投影 Armijo 回溯 + 发散保护；kkt_res_out = 投影梯度范数） */
int mlab_proj_grad_box(const mlab_objective *obj, const double *lb, const double *ub,
                       double *x, int max_iter, double tol, double *kkt_res_out);

/* 对数障碍内点法（路线图 L277）：min f − μ Σ ln(−g_i)，μ 每 outer 轮 ÷10。
   需要严格可行初值（g_i(x)<0），否则返回 -2。λ_i = μ/(−g_i)。 */
int mlab_log_barrier(const mlab_objective *obj,
                     mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                     double mu0, double *x, int max_outer, double tol,
                     double *kkt_res_out);

/* 最小 SQP（路线图 L279 概览级）：有效集 QP 子问题（H=I）+ merit 回溯。 */
int mlab_sqp_min(const mlab_objective *obj,
                 mlab_ineq_fn ineq, int m_ineq, void *ineq_ctx,
                 mlab_eq_fn eq, int m_eq, void *eq_ctx,
                 double *x, int max_iter, double tol, double *kkt_res_out);

#endif /* MLAB_NLP_H */
