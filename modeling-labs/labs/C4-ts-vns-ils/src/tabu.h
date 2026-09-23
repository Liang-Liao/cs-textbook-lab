#ifndef MLAB_TABU_H
#define MLAB_TABU_H

#include "rng.h"
#include "tsp.h"

/* C4 禁忌搜索（Glover）：禁忌表 + 特赦准则，用于小规模 TSP */

typedef struct {
    int tenure;        /* 禁忌任期（迭代步）；swap/2-opt 移动 (i,j) 反向同型 */
    int max_iter;      /* 外层迭代上限 */
    long max_fevals;   /* 长度评估预算；0=不限，靠 max_iter */
    int use_2opt;      /* 1: 2-opt 邻域；0: swap 邻域 */
    double freq_lambda; /* >0: 长期记忆频次惩罚 eff=flen+λ·freq（0=关） */
} mlab_tabu_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_fevals;
    int n_iter;
    int n_tabu_overrides; /* 特赦次数 */
    int n_fallback;       /* 全禁忌时随机逃逸次数 */
    int n_freq_picks;     /* 选中带频次惩罚（freq>0）移动的次数 */
    int tenure_used;
} mlab_tabu_result;

void mlab_tabu_result_free(mlab_tabu_result *r);

/*
 * 禁忌搜索：best-improvement + 等长随机 tie-break；禁忌反向移动，
 * 全局改进时特赦；全禁忌时随机逃逸（更新 f_best 与禁忌表）。
 * freq_lambda>0 时启用长期记忆：移动评估加 λ·选择频次（多样化）。
 * tour0 为初始解（不修改调用方缓冲）。
 */
int mlab_tabu_tsp(mlab_rng *rng,
                  const mlab_tsp_inst *inst,
                  const mlab_tabu_config *cfg,
                  const int *tour0,
                  mlab_tabu_result *out);

#endif
