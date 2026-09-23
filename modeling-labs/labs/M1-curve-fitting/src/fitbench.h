#ifndef MLAB_FITBENCH_H
#define MLAB_FITBENCH_H

#include "lsq.h"
#include "opt.h"

/* M1 曲线拟合工作台：双引擎（LM + BFGS）+ CI + 自动报告 */

enum {
    MLAB_FIT_MODEL_EXP = 0,       /* a exp(-b t) */
    MLAB_FIT_MODEL_EXP_SIN = 1    /* a exp(-b t) sin(c t + d) */
};

typedef struct {
    int model_id;
    int npar;
    const double *theta_true;
    double sigma;
    int m;                 /* 样本数 */
    const double *t;       /* 设计点，长度 m */
    double *y;             /* 含噪观测（输出） */
    double *y_clean;       /* 可空 */
} mlab_fit_data;

/* 模型求值 */
double mlab_fit_model_eval(int model_id, const double *t, const double *theta, int npar, void *ctx);

/* 生成合成数据：t 均匀网格 + 高斯噪声 */
int mlab_fit_synth(mlab_fit_data *d, unsigned seed);

typedef struct {
    double *theta;
    int npar;
    double rss;
    double rmse;
    double sigma2;     /* RSS/(m-npar) */
    double *cov;       /* npar×npar，θ 协方差 */
    double *se;        /* 标准误 */
    int iters;
    int fevals;
    int status;        /* 0 ok */
    const char *engine; /* "lm" | "bfgs" */
} mlab_fit_result;

int mlab_fit_result_alloc(mlab_fit_result *r, int npar);
void mlab_fit_result_free(mlab_fit_result *r);

/* BFGS 引擎：最小化 0.5*RSS，数值梯度 */
int mlab_fit_bfgs(const double *t, const double *y, int m,
                  int model_id, double *theta, int npar,
                  int max_iter, double tol, mlab_fit_result *out);

/* LM 引擎（B3 mlab_lm 封装）+ 协方差 */
int mlab_fit_lm(const double *t, const double *y, int m,
                int model_id, double *theta, int npar,
                int max_iter, double tol, mlab_fit_result *out);

/* 有限差分雅可比 + cov = σ² (JᵀJ)^{-1} */
int mlab_fit_covariance(const double *t, const double *y, int m,
                        int model_id, const double *theta, int npar,
                        double sigma2, double *cov_out, double *se_out);

/* Student-t 97.5% 分位（dof 自由度；dof<=0 或极大退化为正态 1.96）。
 * 经不完全 Beta 函数 I_x(dof/2, 1/2) 数值求逆，仅依赖 libm。 */
double mlab_fit_t_crit95(int dof);

/* CI：theta ± t_{0.975}(dof) se（dof = m - npar） */
int mlab_fit_ci95_contains(const mlab_fit_result *r, const double *theta_true,
                            int dof, int *hit_out);

/* 写一次拟合的报告片段（参数表/协方差/残差） */
int mlab_fit_write_report(const char *path, const char *title,
                          const mlab_fit_data *d, const mlab_fit_result *r,
                          int append);

#endif /* MLAB_FITBENCH_H */
