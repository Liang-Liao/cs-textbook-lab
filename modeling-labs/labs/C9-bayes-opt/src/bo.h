#ifndef MLAB_BO_H
#define MLAB_BO_H

#include "gp.h"
#include "rng.h"

/* C9 贝叶斯优化：LHS 初始化 + EI 采集 + BO 循环；随机/LHS 基线 */

/* 内部栈缓冲上限：超维配置直接判参数错误（守卫，防越界写） */
#define MLAB_BO_MAX_DIM 16

typedef enum {
    MLAB_BO_ACQ_EI = 0,
    MLAB_BO_ACQ_PI = 1,
    MLAB_BO_ACQ_UCB = 2
} mlab_bo_acq;

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int n_init;       /* LHS 初始设计点数 */
    int max_evals;    /* 真评估总预算 */
    mlab_bo_acq acq;
    double ls, sf2, noise, xi, kappa;
    int grid_n;       /* 采集函数网格分辨率（每维） */
    int use_bfgs;     /* 1: 网格 top + BFGS 多起点精修 */
    int bfgs_starts;
} mlab_bo_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int reached;          /* 是否达到 target */
    int evals_to_target;  /* 未达到则 -1 */
    double *best_hist;    /* 每次评估后的 running best */
    int hist_len;
} mlab_bo_result;

void mlab_bo_result_free(mlab_bo_result *r);

/* 拉丁超立方：n 点 × dim，写入 X[n*dim] */
void mlab_lhs(mlab_rng *rng, int n, int dim,
              const double *lb, const double *ub, double *X);

/* 在盒内最大化采集函数（最小化问题的 EI 等）；返回该点处采集值 */
double mlab_bo_maximize_acq(const mlab_gp *gp, mlab_bo_acq acq,
                            double best, double xi, double kappa,
                            const double *lb, const double *ub, int dim,
                            int grid_n, int use_bfgs, int bfgs_starts,
                            double *x_out);

/* BO 循环；target> -1e299 时记录首次达到 target 的评估数 */
int mlab_bo_run(mlab_rng *rng, const mlab_bo_config *cfg,
                double target, mlab_bo_result *out);

/* 批量 BO（q-EI 常量 liar）：每轮 GP 依次提出 q 个点后再统一真评估。
 * 提出第 b>1 个点时，先前 pending 点的未知观测以常数 liar
 * （= 批开始时的当前 best_f）临时并入训练集重拟合，再最大化 EI。
 * 评估数仍逐点计入 n_eval（批量节拍不改变真评估预算语义）。 */
int mlab_bo_run_batch(mlab_rng *rng, const mlab_bo_config *cfg, int q,
                      double target, mlab_bo_result *out);

/* 基线：均匀随机搜索 */
int mlab_bo_random(mlab_rng *rng, const mlab_bo_config *cfg,
                   double target, mlab_bo_result *out);

/* 基线：一次性 LHS 设计按生成顺序评估 */
int mlab_bo_lhs_baseline(mlab_rng *rng, const mlab_bo_config *cfg,
                         double target, mlab_bo_result *out);

#endif
