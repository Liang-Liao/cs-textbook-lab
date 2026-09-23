#ifndef MLAB_E2E_H
#define MLAB_E2E_H

#include "mcmc.h"
#include "rng.h"

/* M3 端到端参数估计与不确定性量化
 * 模型: Lotka-Volterra（默认）；SEIR 预留 model_id
 * 流水线: 合成观测 → M2 式混合点估计 → C2 MCMC → 95% CrI → 报告
 */

enum {
    MLAB_E2E_MODEL_LV = 0,
    MLAB_E2E_MODEL_SEIR = 1
};

#define MLAB_E2E_NPAR 4
#define MLAB_E2E_MAX_OBS 80
#define MLAB_E2E_NCHAIN_MAX 8

/* LV: x'=αx-βxy, y'=δxy-γy；θ=(α,β,γ,δ) */
typedef struct {
    int model_id;
    int npar;
    double theta_true[MLAB_E2E_NPAR];
    double y0[2];          /* 初值（已知） */
    double t0, t1;
    int n_obs;
    double sigma;          /* 两物种观测噪声 */
    double h_ode;          /* RK4 步长 */
    double lb[MLAB_E2E_NPAR];
    double ub[MLAB_E2E_NPAR];
} mlab_e2e_config;

typedef struct {
    double *t;             /* n_obs */
    double *yobs;          /* n_obs*2 行主序 (prey, pred) */
    double *yclean;        /* 可空 */
    int n_obs;
    double sigma;
} mlab_e2e_obs;

typedef struct {
    mlab_chain chains[MLAB_E2E_NCHAIN_MAX];
    int n_chains;
    int d;
    double mean[MLAB_E2E_NPAR];
    double sd[MLAB_E2E_NPAR];
    double cri_lo[MLAB_E2E_NPAR];
    double cri_hi[MLAB_E2E_NPAR];
    double rhat;
    double accept_rate;    /* 各链平均 */
    double step;           /* 缩放空间中的 RWM 步长 */
} mlab_e2e_posterior;

typedef struct {
    const mlab_e2e_obs *obs;
    const double *y0;
    double sigma;
    double h_ode;
    double lb[MLAB_E2E_NPAR];
    double ub[MLAB_E2E_NPAR];
    int model_id;          /* 防呆：nll 按 model_id 分派仿真正本（R7） */
} mlab_e2e_like_ctx;

/* 默认 LV 配置（真参数、噪声、边界） */
void mlab_e2e_default_lv(mlab_e2e_config *cfg);

/* LV 在观测时刻的数值解；y_out 长度 n_obs*2 */
int mlab_e2e_lv_simulate(const double theta[MLAB_E2E_NPAR], const double y0[2],
                         const double *t_obs, int n_obs,
                         double h_ode, double *y_out);

/* SEIR 仿真（预留：D2 模板；估计主线用 LV） */
int mlab_e2e_seir_simulate(const double theta[MLAB_E2E_NPAR], const double y0[2],
                           const double *t_obs, int n_obs,
                           double h_ode, double *y_out);

void mlab_e2e_obs_free(mlab_e2e_obs *obs);

/* 合成观测：真参数积分 + 高斯噪声 */
int mlab_e2e_synth(mlab_rng *rng, const mlab_e2e_config *cfg,
                   mlab_e2e_obs *obs);

/* 高斯似然负对数（不含与 θ 无关的常数项） */
double mlab_e2e_nll(const double *theta, int npar, void *ctx);

/* 未归一化 log 后验 = -NLL + 均匀先验（盒外 -inf） */
double mlab_e2e_logpost(const double *theta, int d, void *ctx);

/* M2 式混合点估计：DE(rand/1) 全局 + 缩小盒 DE(best/1) + Nelder-Mead */
int mlab_e2e_point_est(mlab_rng *rng,
                       double (*f)(const double *, int, void *), void *ctx,
                       int dim, const double *lb, const double *ub,
                       int budget,
                       double *theta_hat, double *f_min, int *n_eval);

/* 多链 RWM-MH（缩放坐标 + burn-in 内步长标定） */
int mlab_e2e_mcmc(mlab_rng *rng,
                  const mlab_e2e_like_ctx *like,
                  const double *theta0,
                  int n_burn, int n_keep, int thin, int n_chains,
                  double step0,
                  mlab_e2e_posterior *post);

void mlab_e2e_posterior_free(mlab_e2e_posterior *post);

/*
 * 全流程：合成 → 点估计 → MCMC → 可选落盘。
 * results_dir 非空时写 obs/theta/samples/report。
 * 返回 0 成功。
 */
int mlab_e2e_pipeline(mlab_rng *rng,
                      const mlab_e2e_config *cfg,
                      unsigned data_seed,
                      unsigned mcmc_seed,
                      int budget,
                      int n_burn, int n_keep, int thin, int n_chains,
                      const char *results_dir,
                      double *theta_hat,
                      double *f_min,
                      int *n_eval,
                      mlab_e2e_obs *obs_out,
                      mlab_e2e_posterior *post_out);

/* 全流程报告（中文） */
int mlab_e2e_write_report(const char *path,
                          const mlab_e2e_config *cfg,
                          const mlab_e2e_obs *obs,
                          const double *theta_hat,
                          double f_min,
                          int n_eval,
                          const mlab_e2e_posterior *post,
                          int append);

#endif /* MLAB_E2E_H */
