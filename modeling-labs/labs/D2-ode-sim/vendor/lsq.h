#ifndef MLAB_LSQ_H
#define MLAB_LSQ_H

/* Linear LS: min ||X beta - y||. X is m x n row-major.
   Methods: normal equations + Cholesky; QR (modified Gram-Schmidt).
   Returns 0 on success; -1 alloc/bad input; QR 在近秩亏（列范数
   < 1e-12×矩阵尺度）时返回 -2（病态分级告警，正规方程则直接失败）。 */
int mlab_lsq_normal(const double *X, int m, int n, const double *y, double *beta);
int mlab_lsq_qr(const double *X, int m, int n, const double *y, double *beta);

/* Residual sum of squares and parameter covariance approx sigma^2 (X^T X)^{-1}. */
double mlab_lsq_rss(const double *X, int m, int n, const double *y, const double *beta);
int mlab_lsq_cov(const double *X, int m, int n, double sigma2, double *cov_out);

/* 决定系数 R^2 = 1 - RSS/TSS（路线图 L233 残差诊断）。 */
double mlab_lsq_r2(const double *X, int m, int n, const double *y, const double *beta);

/* 加权 LS：min Σ w_i (y_i - x_i^T beta)^2，w_i > 0（异方差，路线图 L233）。 */
int mlab_lsq_weighted(const double *X, int m, int n, const double *y,
                      const double *w, double *beta);

/* Huber 稳健回归（IRLS，路线图 L233）：
   残差 |r|<=delta 用二次、>delta 用线性，迭代至权重稳定。
   返回 0 成功；iters_out 可空。 */
int mlab_lsq_huber(const double *X, int m, int n, const double *y,
                   double delta, int max_iter, double *beta, int *iters_out);

/* Nonlinear model: y_i = f(t_i, theta, npar) + noise */
typedef double (*mlab_nl_model)(const double *t, const double *theta, int npar, void *ctx);

/*
 * Levenberg-Marquardt. theta in/out.
 * 返回码：0 = 收敛（RSS 相对改进 < tol）；1 = max_iter 用尽；2 = 内层停滞
 * （连续阻尼放大仍无下降）；-1 = 输入/分配失败。
 * *iters optional.
 */
int mlab_lm(const double *t, const double *y, int m,
            mlab_nl_model model, void *ctx,
            double *theta, int npar, int max_iter, double tol, int *iters_out);

#endif /* MLAB_LSQ_H */
