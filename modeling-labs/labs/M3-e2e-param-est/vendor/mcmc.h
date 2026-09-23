#ifndef MLAB_MCMC_H
#define MLAB_MCMC_H

#include "rng.h"

/* C2: Metropolis-Hastings / Gibbs / 链诊断（IAT、Gelman-Rubin R̂） */

/* 未归一化对数目标密度 log π̃(x) */
typedef double (*mlab_log_density_fn)(const double *x, int d, void *ctx);

typedef struct {
    double *x;      /* n_keep * d，行主序，burn-in 后按 thin 存盘 */
    int d;
    int n_keep;
    int n_burn;
    int thin;
    long n_prop;    /* 总提议次数（含 burn-in） */
    long n_accept;
    double accept_rate;
} mlab_chain;

void mlab_chain_free(mlab_chain *c);

/* 样本均值 / 协方差（链后验矩） */
void mlab_chain_mean(const mlab_chain *c, double *mean_out);
void mlab_chain_cov(const mlab_chain *c, double *cov_out);

/* 维 dim 滞后 lag 的样本自相关；lag=0 → 1 */
double mlab_chain_acf(const mlab_chain *c, int dim, int lag);

/*
 * 积分自相关时间 τ（Geyer 初始正序列 + 配对单调）。
 * max_lag<=0 时用 min(n/2, 500)。
 */
double mlab_chain_iat(const mlab_chain *c, int dim, int max_lag);

/*
 * 经典 Gelman-Rubin R̂。
 * chains[i] 指向第 i 条链的 n*d 样本（行主序），共 m 条等长链。
 * 返回各维 R̂ 的最大值；rhat_per_dim 可空（长度 d）。
 */
double mlab_gelman_rubin(const double *const *chains, int m, int n, int d,
                         double *rhat_per_dim);

/*
 * 随机游走 Metropolis-Hastings：y = x + step * N(0,I)。
 * 对称提议 → 接受率 min(1, π(y)/π(x))。
 */
int mlab_mh_rwm(mlab_rng *rng,
                mlab_log_density_fn log_pi, void *ctx,
                const double *x0, int d,
                double step,
                int n_burn, int n_keep, int thin,
                mlab_chain *out);

/*
 * 独立 MH：提议 N(prop_mu, prop_sigma^2 I)。
 * 接受率 min(1, π(y)q(x)/(π(x)q(y)))。
 */
int mlab_mh_independent(mlab_rng *rng,
                        mlab_log_density_fn log_pi, void *ctx,
                        const double *prop_mu, double prop_sigma,
                        const double *x0, int d,
                        int n_burn, int n_keep, int thin,
                        mlab_chain *out);

/* 梯度回调：grad_out[0..d-1] = ∇log π(x) */
typedef void (*mlab_grad_fn)(const double *x, int d, void *ctx, double *grad_out);

/*
 * HMC 最小实现（路线图 C2 注记项）：动量 p~N(0,I)，leapfrog 积分
 * n_leapfrog 步（步长 step），终点 Metropolis 接受。
 * 用于与 RWM 的 IAT 对照：梯度引导提议在高维更快混合。
 */
int mlab_hmc(mlab_rng *rng,
             mlab_log_density_fn log_pi, mlab_grad_fn grad_log_pi, void *ctx,
             const double *x0, int d,
             double step, int n_leapfrog,
             int n_burn, int n_keep, int thin,
             mlab_chain *out);

/*
 * 二元正态 Gibbs：目标 N(mu, Σ)，Σ=[[s1²,ρ s1 s2],[ρ s1 s2,s2²]]。
 * 条件分布解析，轮转采样。
 */
int mlab_gibbs_bvn(mlab_rng *rng,
                   const double mu[2], double s1, double s2, double rho,
                   const double x0[2],
                   int n_burn, int n_keep, int thin,
                   mlab_chain *out);

/* 二元正态未归一化 log π̃（供 MH 使用；ctx = mlab_bvn_ctx*） */
typedef struct {
    double mu[2];
    double s1, s2, rho;
} mlab_bvn_ctx;

double mlab_bvn_logpdf(const double *x, int d, void *ctx);

/* 共轭正态：先验 μ~N(mu0, s0² I)，似然 x_i~N(μ, Σ_bvn)，Σ 已知 → 后验闭式 */
typedef struct {
    double mu[2];     /* 后验均值 */
    double s1, s2, rho; /* 后验协方差参数化 */
} mlab_bvn_posterior;

void mlab_bvn_conjugate_posterior(const double mu0[2], double s0,
                                  const double s1, const double s2,
                                  const double rho,
                                  const double *data, int n, int d,
                                  mlab_bvn_posterior *out);

#endif /* MLAB_MCMC_H */
