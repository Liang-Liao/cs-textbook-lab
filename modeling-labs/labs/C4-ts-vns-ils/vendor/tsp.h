#ifndef MLAB_TSP_H
#define MLAB_TSP_H

#include "rng.h"
#include "sa.h"

/* C3 小规模欧氏 TSP + swap / 2-opt 邻域 SA */

typedef struct {
    int n;
    double *x;
    double *y;
    double *dist; /* 可选 n*n 对称距离矩阵；非空时优先用矩阵 */
} mlab_tsp_inst;

typedef enum {
    MLAB_TSP_NBHD_SWAP = 0,
    MLAB_TSP_NBHD_2OPT = 1
} mlab_tsp_nbhd;

typedef struct {
    double T0, alpha, T_min;
    int inner;
    int max_outer;
    mlab_tsp_nbhd nbhd;
} mlab_tsp_sa_config;

typedef struct {
    double best_len;
    double final_len;
    int *best_tour;
    int n_fevals;
    int n_accept;
    int n_outer;
    double accept_rate;
} mlab_tsp_sa_result;

int mlab_tsp_random(mlab_rng *rng, mlab_tsp_inst *inst, int n, double box);
/* 非度量对称随机距离矩阵（局部最优更多，供 ILS/VNS 参数实验） */
int mlab_tsp_random_matrix(mlab_rng *rng, mlab_tsp_inst *inst, int n,
                           double dmin, double dmax);
void mlab_tsp_free(mlab_tsp_inst *inst);

/* 闭合回路欧氏长度 */
double mlab_tsp_length(const mlab_tsp_inst *inst, const int *tour);

void mlab_tsp_identity_tour(int n, int *tour);
void mlab_tsp_shuffle_tour(mlab_rng *rng, int n, int *tour);

/* 邻域：交换 i,j 位置城市；2-opt 翻转 (i..j] 段 */
void mlab_tsp_apply_swap(int *tour, int n, int i, int j);
void mlab_tsp_apply_2opt(int *tour, int n, int i, int j);
/* or-opt：把 i 处城市取出，插入到 j 之前 */
void mlab_tsp_apply_oropt(int *tour, int n, int i, int j);

void mlab_tsp_sa_result_free(mlab_tsp_sa_result *r);

int mlab_sa_tsp(mlab_rng *rng,
                const mlab_tsp_inst *inst,
                const mlab_tsp_sa_config *cfg,
                const int *tour0,
                mlab_tsp_sa_result *out);

#endif
