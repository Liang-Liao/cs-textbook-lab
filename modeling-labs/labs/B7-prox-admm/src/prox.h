#ifndef PROX_H
#define PROX_H

/* soft-threshold */
double mlab_soft_threshold(double z, double tau);

/* ISTA / FISTA for LASSO: min 0.5||Xw-y||^2 + lam||w||_1
   X is m x n row-major. Returns objective; w in/out. */
double mlab_ista(const double *X, int m, int n, const double *y,
                 double lam, double *w, int max_iter);
double mlab_fista(const double *X, int m, int n, const double *y,
                  double lam, double *w, int max_iter);

/* ADMM for min f(x)+g(z) s.t. x-z=0, f=0.5||Ax-b||^2, g=lam||z||1 (LASSO split)
   Cholesky of (X'X + rho I) 只分解一次复用；失败返回 -1（目标非负，负值即错误）。 */
double mlab_admm_lasso(const double *X, int m, int n, const double *y,
                       double lam, double *w, int max_iter, double rho);

/* 次梯度法解 LASSO（路线图 L327/L334）：w <- w - alpha_k (X'(Xw-y) + lam sign(w))，
   alpha_k = alpha0/sqrt(k)。返回目标值。 */
double mlab_subgradient_lasso(const double *X, int m, int n, const double *y,
                              double lam, double *w, int max_iter, double alpha0);

/* 近端点法（路线图 L328）：w_{k+1} = prox_{alpha*f}(w_k)，
   内层用近端 ISTA 迭代求解。返回目标值。 */
double mlab_prox_point_lasso(const double *X, int m, int n, const double *y,
                             double lam, double *w, int outer, int inner,
                             double alpha);

/* 基追踪 BP（路线图 L331）：min ||w||_1 s.t. Xw = y（ADMM，Cholesky 一次）。
   返回 ||w||_1；失败返回 -1。 */
double mlab_bp_admm(const double *X, int m, int n, const double *y,
                    double *w, int max_iter, double rho);

/*
 * ADMM 解带等式约束的箱内 QP（路线图 L336/341）：
 *   min 0.5||x||^2  s.t. A x = b（A 为 m x n 行主序）
 * x-update 解 (I+rho I) 三对角常数矩阵（=标量对角），z-update 为仿射集投影。
 * 返回 0 收敛（原始/对偶残差 < tol）；1 达到 max_iter；-1 分解失败。
 */
int mlab_admm_qp_eq(const double *A, int m, int n, const double *b,
                    double rho, int max_iter, double tol, double *x_out);

#endif /* PROX_H */
