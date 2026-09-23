#ifndef MLAB_CUTS_H
#define MLAB_CUTS_H

/*
 * 割平面最小演示（路线图 B4 L257 "割平面/分支切割概览" 的可运行落点）：
 * 2 维纯整数规划  max c1 x1 + c2 x2  s.t. A x <= b（整数系数），x 整数 >= 0。
 * LP 松弛用顶点枚举求解；最优为分数顶点时，从其基（两条紧约束）出发
 * 生成 Gomory 割加入约束，迭代。返回割平面法的最优值与解。
 * 返回 0 = 得到整数最优（*n_cuts_out 为所用割数）；1 = 达到割数上限仍未闭环。
 */
int mlab_cuts_demo_2d(const double *A, const double *b, const double *c, int m,
                      double *x_out, double *obj_out, int *n_cuts_out);

#endif /* MLAB_CUTS_H */
