#ifndef MLAB_CMAES_H
#define MLAB_CMAES_H

#include "rng.h"

/* C6: 简化可运行 CMA-ES（进化路径 + 秩-μ + σ 自适应 + Jacobi 特征分解） */

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int max_evals;      /* 评估预算 */
    double sigma0;      /* 初始步长 */
    double stop_f;      /* >0 时 best_f < stop_f 提前停；记录 evals */
    double x0_scale;    /* 无 x0 时 m0 各坐标 ~ U(-s,s) */
    int track_angle;    /* dim==2 时记录主轴夹角（度） */
    double major_axis[2]; /* 等高线长轴方向（单位向量）；track_angle 用 */
    int lambda_mul;     /* 种群倍率 λ←λ×mul（IPOP 重启用）；<=1 → 1 */
} mlab_cmaes_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int gen_used;
    int reached;            /* 是否达到 stop_f */
    int evals_to_target;    /* 未达到则 -1 */
    double *best_hist;      /* 每代 best_f */
    double *sigma_hist;
    double *angle_hist;     /* 度，track_angle 时有效 */
    int hist_len;
    double *principal;      /* 最终 C 最大特征向量（长度 dim） */
    double cond_C;          /* C 特征值 sqrt 条件数 dmax/dmin */
} mlab_cmaes_result;

void mlab_cmaes_result_free(mlab_cmaes_result *r);

/* 对称矩阵 Jacobi 特征分解：A 对称 n×n（破坏性），evals 升序，V 列为特征向量 */
int mlab_symeig_jacobi(double *A, int n, double *evals, double *V);

/* v 与 target 的夹角（度），|cos|，范围 [0,90] */
double mlab_axis_angle_deg(const double *v, const double *target, int n);

int mlab_cmaes_run(mlab_rng *rng, const mlab_cmaes_config *cfg,
                   const double *x0, mlab_cmaes_result *out);

/* IPOP 重启：总预算内若未达 stop_f，则 λ 倍增（1,2,4,…）重启，取全程最优。
 * out->n_eval 为累计评估数；reached/evals_to_target 相对总起点计。 */
int mlab_cmaes_run_ipop(mlab_rng *rng, const mlab_cmaes_config *cfg,
                        mlab_cmaes_result *out);

#endif
