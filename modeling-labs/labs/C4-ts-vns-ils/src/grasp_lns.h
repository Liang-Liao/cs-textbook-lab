#ifndef MLAB_GRASP_LNS_H
#define MLAB_GRASP_LNS_H

#include "rng.h"
#include "tsp.h"
#include "vns_ils.h"

/* C4 注记项落地：LNS（毁灭-重建）与 GRASP（贪心随机自适应 + 局部搜索） */

/* ---- LNS：大邻域搜索，随机移除 destroy_count 城后最小代价重插 ---- */

typedef struct {
    int destroy_count;   /* 每次迭代移除的城市数（≥1） */
    unsigned nbhd_mask;  /* 修复后局部搜索邻域（0 = 不做局部搜索） */
    int max_fevals;      /* 评估预算；重插每城计 1 次评估（≈一条边扫描） */
    int max_outer;       /* 迭代上限；0→由预算推 */
    int ls_pass_limit;   /* 单次局部搜索上限 */
} mlab_lns_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_fevals;
    int n_outer;
    int n_accept; /* 修复解被接受次数 */
} mlab_lns_result;

void mlab_lns_result_free(mlab_lns_result *r);

int mlab_lns_tsp(mlab_rng *rng,
                 const mlab_tsp_inst *inst,
                 const mlab_lns_config *cfg,
                 const int *tour0,
                 mlab_lns_result *out);

/* ---- GRASP：受限候选表（RCL）随机贪心构造 + 首次改进局部搜索 ---- */

typedef struct {
    double rcl_alpha;   /* RCL 松紧：0=纯贪心 NN，1=纯随机 */
    int n_iter;         /* 构造+局部搜索 迭代次数 */
    unsigned nbhd_mask; /* 局部搜索邻域 */
    int max_fevals;     /* 评估预算；每次构造计 1 次评估 */
    int ls_pass_limit;
} mlab_grasp_config;

typedef struct {
    double best_len;
    int *best_tour;
    int n_fevals;
    int n_iter;
} mlab_grasp_result;

void mlab_grasp_result_free(mlab_grasp_result *r);

int mlab_grasp_tsp(mlab_rng *rng,
                   const mlab_tsp_inst *inst,
                   const mlab_grasp_config *cfg,
                   mlab_grasp_result *out);

#endif
