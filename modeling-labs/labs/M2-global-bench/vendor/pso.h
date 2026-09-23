#ifndef MLAB_PSO_H
#define MLAB_PSO_H

#include "rng.h"

/* C7 粒子群：惯性 w、认知/社会系数、星型/环型拓扑、消融模式 */

typedef enum {
    MLAB_PSO_TOPO_GLOBAL = 0, /* 全局星型：邻域 = 全局最优 */
    MLAB_PSO_TOPO_RING = 1,   /* 环型：邻域 = 左右邻居中的最优 */
    MLAB_PSO_TOPO_VON_NEUMANN = 2 /* Von Neumann：二维网格上下左右邻居 */
} mlab_pso_topo;

typedef enum {
    MLAB_PSO_MODE_FULL = 0,     /* 惯性 + 认知 + 社会 */
    MLAB_PSO_MODE_COG_ONLY = 1, /* 社会系数 = 0 */
    MLAB_PSO_MODE_SOC_ONLY = 2  /* 认知系数 = 0 */
} mlab_pso_mode;

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int swarm;           /* 粒子数 */
    int max_gen;
    double w;            /* 惯性权重（w_linear 时为初始值） */
    int w_linear;        /* 1: w 线性降到 w_end（Shi-Eberhart） */
    double w_end;
    double c1, c2;       /* 认知 / 社会加速系数；模式会覆盖 */
    double vmax_scale;   /* v_max = vmax_scale * (ub-lb)，默认 0.2 */
    mlab_pso_topo topo;
    mlab_pso_mode mode;
    int use_constriction; /* 1: Clerc 收缩因子 χ（经典 χ=0.729, c1=c2=1.4962，
                             w=1，不再用 vmax 钳制） */
    double chi;           /* <=0 → 0.729 */
    double stop_f;       /* >0 时 best < stop_f 提前停 */
} mlab_pso_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int gen_used;
    int reached;
    double *best_hist;
    int hist_len;
} mlab_pso_result;

void mlab_pso_result_free(mlab_pso_result *r);

int mlab_pso_run(mlab_rng *rng, const mlab_pso_config *cfg,
                 const double *x0_swarm, /* 可空；否则盒内均匀 */
                 mlab_pso_result *out);

const char *mlab_pso_topo_name(mlab_pso_topo t);
const char *mlab_pso_mode_name(mlab_pso_mode m);

#endif
