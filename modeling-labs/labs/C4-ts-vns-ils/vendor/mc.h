#ifndef MLAB_MC_H
#define MLAB_MC_H

#include "rng.h"

/* MC 积分 / 拒绝采样 / 重要性采样（C1 自生长，C2–M* 复用） */

typedef struct {
    double mean;   /* 积分估计 */
    double var;    /* 被积贡献样本方差 */
    double se;     /* mean 的标准误 sqrt(var/n) */
    long n;        /* 使用样本数 */
} mlab_mc_result;

typedef double (*mlab_mc_box_fn)(const double *x, int d, void *ctx);
typedef int (*mlab_mc_ind_fn)(const double *x, int d, void *ctx);
typedef double (*mlab_scalar_fn)(double x, void *ctx);
typedef double (*mlab_sample_fn)(mlab_rng *r, void *ctx);

/* 均匀盒 MC：X~U(lb,ub)^d，返回 volume * mean(f) */
double mlab_mc_box(mlab_rng *r, mlab_mc_box_fn f, void *ctx, int d,
                   const double *lb, const double *ub, long n,
                   mlab_mc_result *out);

/* 命中-未中：估计 P(inside)，返回概率估计 */
double mlab_mc_hit_miss(mlab_rng *r, mlab_mc_ind_fn inside, void *ctx, int d,
                        const double *lb, const double *ub, long n,
                        mlab_mc_result *out);

/*
 * 一维重要性采样：估计 ∫ f，提议密度 q，估计量 mean(f(X)/q(X))，X~q。
 * 适合定义在有限区间外为 0 的被积函数。
 */
double mlab_mc_is(mlab_rng *r,
                  mlab_scalar_fn f, void *fctx,
                  mlab_scalar_fn q_density, void *qctx,
                  mlab_sample_fn q_sample, void *sctx,
                  long n, mlab_mc_result *out);

/* IS 权重诊断（路线图 C1：权重方差与坏比值）。 */
typedef struct {
    double ess;         /* 有效样本量 (Σw)²/Σw²，w=p/q（未归一化） */
    double ess_ratio;   /* ess/n ∈ (0,1]，越接近 1 提议越好 */
    double w_cv2;       /* 权重变异系数平方 Var(w)/E[w]² */
    double w_max_ratio; /* max w / Σ w（坏比值：单样本占比过高） */
} mlab_mc_is_diag;

/*
 * 带权重诊断的一维 IS：估计 E_p[f]，X~q，权重 w=p/q。
 * p_density 需给出目标密度（归一化常数可并入 f）。
 * diag 可空。返回估计值，out 携带 mean/var/se。
 */
double mlab_mc_is_weighted(mlab_rng *r,
                           mlab_scalar_fn p_density, void *pctx,
                           mlab_scalar_fn f, void *fctx,
                           mlab_scalar_fn q_density, void *qctx,
                           mlab_sample_fn q_sample, void *sctx,
                           long n, mlab_mc_result *out,
                           mlab_mc_is_diag *diag);

/*
 * 拒绝采样一次成功抽取；返回 1 成功、0 超过 max_trials。
 * c 满足 target(x) <= c * env_pdf(x)（在支撑集上）。
 * 接受准则：U * c * env_pdf(Y) <= target(Y)，Y ~ env_sample。
 */
int mlab_rejection_sample(mlab_rng *r,
                          mlab_scalar_fn target, void *tctx,
                          mlab_sample_fn env_sample, void *sctx,
                          mlab_scalar_fn env_pdf, void *ectx,
                          double c, long max_trials,
                          double *out, long *n_trials);

#endif /* MLAB_MC_H */
