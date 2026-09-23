#ifndef MLAB_ACO_H
#define MLAB_ACO_H

#include "rng.h"
#include "tsp.h"

/* C7 蚁群：信息素 + 启发式转移；精英沉积；可选 2-opt 精修 */

typedef struct {
    int n_ants;
    int max_iter;
    double alpha;    /* 信息素指数 */
    double beta;     /* 启发式指数 (1/d) */
    double rho;      /* 蒸发系数 */
    int elitist;     /* 1: 仅全局最优沉积（MMAS 风格） */
    int mmas;        /* 1: MMAS 动态上下界（τmax=1/(ρ·L_best)，τmin=τmax/2n），
                         优先于 elitist */
    int use_2opt;    /* 1: 对每只蚂蚁回路做 2-opt 精修 */
    int two_opt_pass;/* 2-opt 最大改进轮数，默认 2 */
    double tau0;     /* 初始信息素；<=0 则由贪心长度估计 */
} mlab_aco_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_tours;        /* 构造的回路数（构造评估口径） */
    int n_2opt_evals;   /* 2-opt 精修中的长度评估数（精修口径，单列） */
    int iter_used;
} mlab_aco_result;

void mlab_aco_result_free(mlab_aco_result *r);

/* 多起点最近邻贪心；返回最优长度，tour_out 可空 */
double mlab_tsp_greedy_nn(const mlab_tsp_inst *inst, mlab_rng *rng,
                          int n_starts, int *tour_out);

/* 就地 2-opt 精修到局部最优（最多 max_pass 轮全邻域扫描） */
double mlab_tsp_two_opt_refine(const mlab_tsp_inst *inst, int *tour,
                               int max_pass, int *n_eval);

int mlab_aco_tsp(mlab_rng *rng, const mlab_tsp_inst *inst,
                 const mlab_aco_config *cfg, mlab_aco_result *out);

#endif
