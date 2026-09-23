#ifndef MLAB_MOEAD_H
#define MLAB_MOEAD_H

#include "nsga2.h" /* mlab_mo_eval、mlab_igd */
#include "rng.h"

/* C8 MOEA/D 最小实现（Tchebycheff 分解，2 目标）：
 * 权重均匀分布 + 邻域交配 + 理想点 z 动态更新 */

typedef struct {
    mlab_mo_eval eval;   /* 复用 nsga2.h 的多目标评估接口 */
    void *ctx;
    int dim;
    int nobj;            /* 本实现按 2 目标 Tchebycheff 处理 */
    const double *lb, *ub;
    int pop;             /* 子问题数（权重向量数） */
    int max_gen;
    int T;               /* 邻域大小，默认 5 */
    double p_mate_global;/* 全局交配概率 δ（标准 MOEA/D 探索分量）；<=0 → 0.1 */
    double p_cross, eta_c;
    double p_mut, eta_m;
} mlab_moead_config;

typedef struct {
    int pop;
    int dim;
    int nobj;
    int gen_used;
    int n_eval;
    double *X;           /* pop * dim，末代各子问题的解 */
    double *F;           /* pop * nobj */
    double *igd_hist;    /* 每代 IGD（调用方提供 ref 时） */
    int igd_len;
} mlab_moead_result;

void mlab_moead_result_free(mlab_moead_result *r);

int mlab_moead_run(mlab_rng *rng, const mlab_moead_config *cfg,
                   const double *igd_ref, int nref,
                   mlab_moead_result *out);

#endif
