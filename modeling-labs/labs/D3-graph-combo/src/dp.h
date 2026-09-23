#ifndef MLAB_DP_H
#define MLAB_DP_H

/*
 * D3 组合优化精确解（多项式 DP / 小规模枚举）：
 * 作为启发式算法的对账基准（路线图 :651 「多项式精确解 = 启发式对账基准」），
 * 并与 B4 vendor 的分支定界同题联动对账（:649）。
 */

/*
 * 0-1 背包动态规划（整数容量）。
 * w/v 长度 n；容量 cap。重量或容量非整数（|x-round|>1e-9）返回 -1。
 * x_sel 可空，写入 0/1 选择向量。返回最优价值。
 */
double mlab_knapsack_dp(const double *w, const double *v, int n, double cap,
                        int *x_sel);

/*
 * 指派问题暴力枚举全排列（n<=8）：min Σ cost[i][perm[i]]。
 * perm_out 可空。返回最优总代价。
 */
double mlab_assign_brute(const double *cost, int n, int *perm_out);

#endif /* MLAB_DP_H */
