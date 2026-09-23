#ifndef MLAB_ES_H
#define MLAB_ES_H

#include "rng.h"

/* C6: (1+1)-ES + Rechenberg 1/5 成功规则 */

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int max_gen;
    double sigma0;
    double p_target;    /* 默认 0.2 */
    int window;         /* 成功率滑动窗口，默认 20 */
    double adapt_factor; /* 成功率偏离时的乘性因子，默认 1.22 */
    double x0_scale;    /* 无 x0 时初始点各坐标 ~ U(-s,s)，默认 2 */
} mlab_es1p1_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int gen_used;
    double *sigma_hist;
    double *success_hist; /* 每代 0/1 */
    int hist_len;
    double success_rate_warm;  /* 前半段（预热）成功率 */
    double success_rate_late;  /* 后半段成功率 */
    double sigma_final;
} mlab_es1p1_result;

void mlab_es1p1_result_free(mlab_es1p1_result *r);

/* 1/5 规则自适应 (1+1)-ES；返回 1 成功 */
int mlab_es1p1_run(mlab_rng *rng, const mlab_es1p1_config *cfg,
                   const double *x0, mlab_es1p1_result *out);

/* ---------- (μ/ρ,λ)-ES：多父代重组 + comma/plus 选择压力 ---------- */

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int mu;          /* 父代数 μ >= 1 */
    int rho;         /* 参与重组的父代数 1 <= ρ <= μ */
    int lambda_n;    /* 子代数 λ（comma 选择要求 λ >= μ） */
    int plus;        /* 1 → (μ/ρ+λ)；0 → (μ/ρ,λ) */
    int max_gen;
    double sigma0;
    double tau;      /* σ 的 log-normal 学习率；<=0 → 1/sqrt(2·dim) */
    double x0_scale; /* 初始父代各坐标 ~ U(-s,s) */
} mlab_es_murl_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int gen_used;
    double *pop_best_hist; /* 每代选择后父代种群最优（plus 单调，comma 可回退） */
    double *sigma_hist;    /* 每代父代平均 σ */
    int hist_len;
    double sigma_final;
} mlab_es_murl_result;

void mlab_es_murl_result_free(mlab_es_murl_result *r);

/* (μ/ρ,λ)/(μ/ρ+λ)-ES：中间重组 + log-normal σ 自适应；返回 1 成功 */
int mlab_es_murl_run(mlab_rng *rng, const mlab_es_murl_config *cfg,
                     mlab_es_murl_result *out);

#endif
