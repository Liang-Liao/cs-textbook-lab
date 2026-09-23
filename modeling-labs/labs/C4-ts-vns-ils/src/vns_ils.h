#ifndef MLAB_VNS_ILS_H
#define MLAB_VNS_ILS_H

#include "rng.h"
#include "tsp.h"

/* C4：变邻域搜索 VNS / VND 与迭代局部搜索 ILS（TSP）；
 * LNS（毁灭-重建）与 GRASP（贪心随机自适应+局部搜索）见 grasp_lns.h */

/* 邻域族：bit0=swap, bit1=2opt, bit2=or-opt（单城重定位） */
#define MLAB_NB_SWAP 1u
#define MLAB_NB_2OPT 2u
#define MLAB_NB_OROPT 4u
#define MLAB_NB_BOTH 3u
#define MLAB_NB_ALL 7u

typedef struct {
    unsigned nbhd_mask; /* 使用的邻域族 */
    int max_fevals;     /* 长度评估预算 */
    int max_outer;      /* 外层（shake 循环）上限；0→由预算推 */
    int ls_pass_limit;  /* 局部搜索单次最多评估；防卡死 */
    int vnd;            /* 1: 局部搜索用 VND（swap→2opt→…） */
    int kmax;           /* 震颤强度上限 k；<=0 → 默认 5 */
} mlab_vns_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_fevals;
    int n_outer;
    int n_local; /* 完成的局部搜索次数 */
} mlab_vns_result;

void mlab_vns_result_free(mlab_vns_result *r);

/*
 * 首次改进局部搜索；返回是否发生改进。
 * max_evals：本次调用内允许的评估数（≤0 不限）——局部搜索按本次调用内
 * 计数，勿与全局累计 *fevals 混比；*fevals 仍全局累计供调用方记账。
 */
int mlab_tsp_first_improve(const mlab_tsp_inst *inst, int *tour, double *len,
                           unsigned nbhd_mask, int max_evals, int *fevals);

/* 最优改进局部搜索：扫全邻域取最优移动应用一次；预算语义同上 */
int mlab_tsp_best_improve(const mlab_tsp_inst *inst, int *tour, double *len,
                          unsigned nbhd_mask, int max_evals, int *fevals);

/* VND：按 mask 中邻域轮换至局部最优；max_evals 为本次 VND 调用总上限 */
int mlab_tsp_vnd(const mlab_tsp_inst *inst, int *tour, double *len,
                 unsigned nbhd_mask, int max_evals, int *fevals);

/*
 * VNS：shake(k) + 局部搜索；改进则 k 回 1，否则 k++。
 * kmax 由 cfg->kmax 指定（<=0 → 5）。单次局部搜索预算 =
 * min(ls_pass_limit, 剩余全局预算)。
 */
int mlab_vns_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_vns_config *cfg,
                 const int *tour0,
                 mlab_vns_result *out);

typedef struct {
    int pert_strength;  /* 扰动邻域移动次数（swap 次数） */
    unsigned nbhd_mask; /* 局部搜索邻域 */
    int max_fevals;
    int max_outer;
    int ls_pass_limit;
    int accept_worse;   /* 1: 以概率接受更差解（可选）；0: 只接受改进 */
} mlab_ils_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_fevals;
    int n_outer;
    int n_accept;
} mlab_ils_result;

void mlab_ils_result_free(mlab_ils_result *r);

int mlab_ils_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_ils_config *cfg,
                 const int *tour0,
                 mlab_ils_result *out);

#endif
