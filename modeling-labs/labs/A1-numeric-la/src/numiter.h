#ifndef MLAB_NUMITER_H
#define MLAB_NUMITER_H

/*
 * 定常迭代法求解 A x = b（A 为 n x n 行主序，要求对角元 a_ii != 0）。
 * 停机准则：前后两步解的无穷范数增量 max_i |x_new_i - x_old_i| < tol，
 * 或达到 max_iter 次扫描。iters_out（可空）接收实际扫描次数。
 * 返回：0 = 收敛；1 = 达到 max_iter 仍未达 tol；-1 = 输入非法（n<=0/零对角/空指针）。
 */
int mlab_jacobi_solve(const double *A, int n, const double *b, double *x,
                      int max_iter, double tol, int *iters_out);
int mlab_gauss_seidel_solve(const double *A, int n, const double *b, double *x,
                            int max_iter, double tol, int *iters_out);
/* SOR：ω=1 时退化为 Gauss-Seidel；0<ω<2 收敛必要（SPD 充分） */
int mlab_sor_solve(const double *A, int n, const double *b, double omega, double *x,
                   int max_iter, double tol, int *iters_out);

/*
 * Jacobi 迭代矩阵 M = D^{-1}(L+U) 的谱半径估计（幂迭代，定长 500 次，起始向量
 * 由固定 xorshift 决定，确定性可复现）。M 对称（如 D 为纯量倍的对称矩阵）时
 * 收敛到精确谱半径；一般矩阵该值是大范数增长率的上界近似。
 * 返回 rho（>=0）；输入非法返回 -1。
 */
double mlab_jacobi_spectral_radius(const double *A, int n);

#endif /* MLAB_NUMITER_H */
