#ifndef MLAB_ASSIGN_H
#define MLAB_ASSIGN_H

/*
 * 指派问题（路线图 B4 L262 实验 3 的载体之一）：
 * n 个任务指派给 n 个人，cost 为 n×n 行主序，min Σ cost[i][perm[i]]。
 */

/* 分支定界（下界 = 已花代价 + 未指派各行的最小可得代价）。n<=10。 */
double mlab_assign_bnb(const double *cost, int n, int *perm_out);

/* 暴力枚举全排列对账。n<=8。 */
double mlab_assign_enum(const double *cost, int n, int *perm_out);

#endif /* MLAB_ASSIGN_H */
