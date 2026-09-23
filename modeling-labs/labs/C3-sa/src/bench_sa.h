#ifndef MLAB_BENCH_SA_H
#define MLAB_BENCH_SA_H

/* C3 连续测试函数（Rastrigin 等）；后续 lab 可 vendor */

/* f(x)=10n + Σ(x_i² - 10 cos(2π x_i))，全局最优 x*=0, f*=0，定义域常用 [-5.12,5.12] */
double mlab_rastrigin(const double *x, int n, void *ctx);

/* 2D Rastrigin 建议盒约束 */
void mlab_rastrigin_bounds(int n, double *lb, double *ub);

#endif
