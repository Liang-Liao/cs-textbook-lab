#ifndef MLAB_STATS_H
#define MLAB_STATS_H

double mlab_mean(const double *x, int n);
double mlab_var(const double *x, int n);          /* unbiased, n-1 */
double mlab_std(const double *x, int n);
double mlab_cov(const double *x, const double *y, int n);

/* sort ascending in-place (simple qsort wrapper on buffer) */
void mlab_sort_asc(double *x, int n);

/* empirical CDF value F_n(x) for sorted sample xs of size n */
double mlab_ecdf(const double *xs_sorted, int n, double x);

/* ---- 估计量与置信区间（路线图 A2 知识点，B3 的伏笔） ---- */

/* 正态总体的最大似然估计：mu=样本均值，sigma 用 1/n（MLE 口径，非无偏） */
void mlab_mle_normal(const double *x, int n, double *mu_out, double *sigma_out);
/* 已知 sigma 时均值的 z 置信区间：xbar ± z·sigma/sqrt(n)。z=1.96 即 95%。返回 0 成功 */
int mlab_ci_mean_z(const double *x, int n, double sigma, double z,
                   double *lo_out, double *hi_out);

#endif /* MLAB_STATS_H */
