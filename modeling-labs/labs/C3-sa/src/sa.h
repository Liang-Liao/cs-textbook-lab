#ifndef MLAB_SA_H
#define MLAB_SA_H

#include "rng.h"

/* C3 模拟退火：Metropolis + 几何降温 + 连续/组合邻域 */

typedef struct {
    int dim;
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    const double *lb; /* 可空 → 无界 */
    const double *ub;
} mlab_sa_problem;

/* 温度调度（路线图 C3：几何实用 / 对数有理论保证但慢） */
enum {
    MLAB_SA_SCHEDULE_GEOMETRIC = 0,
    MLAB_SA_SCHEDULE_LOG = 1   /* T_k = T0·ln2 / ln(1+k) */
};

typedef struct {
    double T0;           /* 初始温度；若 calibrate_T0>0 则运行前重标定 */
    double alpha;        /* 几何降温 T ← αT，(0,1) */
    int inner;           /* 每温度内循环步数 */
    double T_min;        /* 终止温度 */
    double step_scale;   /* 提议 σ = step_scale * range *（默认 ×T/T0） */
    int calibrate_T0;    /* 1: 用目标接受率反推 T0 */
    double target_accept;/* 标定目标接受率，默认 0.8 */
    int max_outer;       /* 安全上限；0 → 由 T_min/alpha 推（对数调度必填） */
    int step_fixed;      /* 1: σ 不随 T 缩小（接受率-T 诊断用） */
    int schedule;        /* MLAB_SA_SCHEDULE_*；默认几何 */
    int reheat_every;    /* >0: 每 reheat_every 层 T ← reheat_frac·T0；0=关 */
    double reheat_frac;  /* 重加热比例，(0,1]，默认 0.5 */
} mlab_sa_config;

typedef struct {
    double *x_best;
    double f_best;
    double f_final;
    int n_fevals;
    int n_accept;
    int n_outer;         /* 完成的温度层数 */
    double T0_used;
    double accept_rate;  /* 总体 n_accept / 提议次数 */
} mlab_sa_result;

void mlab_sa_result_free(mlab_sa_result *r);

/* Metropolis 接受：df=f_new-f_old，T>0；返回 1 接受 */
int mlab_sa_metropolis(double df, double T, mlab_rng *rng);

/*
 * 初始温度标定（目标接受率反推法）：从 x0 做 n_probe 次邻域试探，
 * 取上坡 Δf 均值，令 exp(-Δ/T0) ≈ target_accept ⇒ T0 = Δ / (-log(target_accept))。
 * step_scale 同 SA 配置（首层 σ = step_scale·range 与试探一致）。
 */
double mlab_sa_calibrate_T0(mlab_rng *rng,
                            const mlab_sa_problem *prob,
                            const double *x0,
                            double step_scale,
                            double target_accept,
                            int n_probe);

/* 连续盒约束 SA（高斯扰动，幅度随 T 线性衰减） */
int mlab_sa_continuous(mlab_rng *rng,
                       const mlab_sa_problem *prob,
                       const mlab_sa_config *cfg,
                       const double *x0,
                       mlab_sa_result *out);

/*
 * 可选诊断缓冲：若 diag_T/diag_acc 非空且容量 diag_cap 足够，
 * 写入每个温度层结束后的 (T, 该层接受率)。返回实际写入层数。
 */
int mlab_sa_continuous_diag(mlab_rng *rng,
                            const mlab_sa_problem *prob,
                            const mlab_sa_config *cfg,
                            const double *x0,
                            mlab_sa_result *out,
                            double *diag_T, double *diag_acc, int diag_cap);

/*
 * 多重启（路线图 C3：重加热与多重启）：从盒内均匀随机起点独立跑
 * n_restarts 次 SA，返回最优一次的结果（其余结果已释放）。
 * lb/ub 至少一者为 NULL 时要求 prob 已带边界。
 */
int mlab_sa_multirestart(mlab_rng *rng,
                         const mlab_sa_problem *prob,
                         const mlab_sa_config *cfg,
                         int n_restarts,
                         mlab_sa_result *out);

#endif
