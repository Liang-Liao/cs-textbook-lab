#ifndef MLAB_LINESEARCH_H
#define MLAB_LINESEARCH_H

#include "opt.h"

typedef double (*mlab_scalar_fn_1d)(double alpha, void *ctx);

/*
 * Golden-section search on [a,b] for unimodal f. Fills *fmin_out (可空).
 * widths_out（可空）：若非空则按迭代记录区间宽度 widths[it] = b-a（调用方需
 * 保证容量 >= max_iter；提前达到 tol 时剩余项保持调用方初值）。
 * Returns x*.
 */
double mlab_golden_section(mlab_scalar_fn_1d f, void *ctx, double a, double b,
                           int max_iter, double tol, double *fmin_out,
                           double *widths_out);

/*
 * Armijo backtracking: phi(alpha)=f(x+alpha*d), phi'(0)=g·d 必须 < 0。
 * 返回值语义：
 *   >0  满足 Armijo 的步长
 *    0  失败——方向非下降（g·d >= 0）或 max_back 次内找不到可接受步长；
 *       调用方应将 0 视为"本次不前进"。
 */
double mlab_armijo_backtrack(const mlab_objective *obj, const double *x,
                             const double *d, double alpha0,
                             double c1, int max_back, int *fevals_out);

/*
 * Strong Wolfe line search（Nocedal & Wright Alg 3.5–3.6，zoom 用二分）。
 * 条件：phi(a) <= phi0 + c1·a·phi'(0)（Armijo）且 |phi'(a)| <= c2·|phi'(0)|，
 * 需 0 < c1 < c2 < 1，且 g·d < 0。
 * 返回可接受步长；方向非下降 / 失败返回 0.0。fe/ge 计数可空。
 */
double mlab_wolfe_strong(const mlab_objective *obj, const double *x,
                         const double *d, double alpha0,
                         double c1, double c2, int max_iter,
                         int *fevals_out, int *gevals_out);

/* Steepest descent exact LS on SPD quadratic f=0.5 x^T A x (x*=0). */
int mlab_steepest_descent_quad(const double *A, int n, double *x,
                               int max_iter, double tol,
                               double *res_hist, int *iter_out, int *fevals_out);

#endif /* MLAB_LINESEARCH_H */
