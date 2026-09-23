#ifndef MLAB_TSP_H
#define MLAB_TSP_H

#define MLAB_TSP_INF 1e300

/* D3 TSP：最近邻贪心、2-opt 局部搜索、Held-Karp 精确 DP */

typedef struct {
    int n;
    double *x, *y;
    double *dist; /* n*n 对称距离矩阵 */
} mlab_tsp;

int mlab_tsp_euclidean(mlab_tsp *t, const double *x, const double *y, int n);
int mlab_tsp_from_matrix(mlab_tsp *t, const double *dist, int n);
void mlab_tsp_free(mlab_tsp *t);
double mlab_tsp_tour_length(const mlab_tsp *t, const int *tour);

/* 最近邻贪心：start 城市；tour 输出长度 n */
double mlab_tsp_nearest_neighbor(const mlab_tsp *t, int start, int *tour);

/* 2-opt 改进（best-improvement）：从 tour0 复制后优化；返回终长度 */
double mlab_tsp_two_opt(const mlab_tsp *t, const int *tour0, int *tour_out,
                        int *n_improve);

/* NN + 2-opt（多起点 NN） */
double mlab_tsp_nn_two_opt(const mlab_tsp *t, int *tour_out);

/*
 * Held-Karp 精确解 O(n^2 2^n)。
 * n<=20 建议；tour 可空（只要长度）。返回最优长度，失败返回 -1。
 */
double mlab_tsp_held_karp(const mlab_tsp *t, int *tour);

#endif /* MLAB_TSP_H */
