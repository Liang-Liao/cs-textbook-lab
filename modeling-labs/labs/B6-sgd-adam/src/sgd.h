#ifndef sgd_H
#define sgd_H

#include "rng.h"

typedef struct {
    int dim;
    double (*loss)(const double *w, const double *x, const double *y, int n, void *ctx);
    void (*grad)(const double *w, const double *x, const double *y, int n,
                 double *g, void *ctx);
    void *ctx;
} sgd_problem;

typedef struct {
    double *m; /* momentum */
    double *v; /* adam second moment */
    double t;
} sgd_state;

int sgd_state_init(sgd_state *st, int dim);
void sgd_state_free(sgd_state *st);

/* kind: 0=SGD, 1=momentum, 2=nesterov, 3=adagrad, 4=rmsprop, 5=adam, 6=adamw */
int sgd_step(const sgd_problem *p, double *w, sgd_state *st,
                  const double *X, const double *Y, int m, int dim,
                  int batch, double lr, int kind, double wd);

/* 学习率调度（路线图 L307-309）：0=常数, 1=∝1/k, 2=∝1/√k */
typedef struct {
    int kind;
    double lr0;
} sgd_lr_sched;

double sgd_sched_lr(const sgd_lr_sched *s, long k);

/*
 * 随机 mini-batch epoch 驱动：epoch 开始用 rng 做 Fisher-Yates 洗牌
 * （固定 seed → 完全可复现），随后按洗牌顺序取连续 batch 个样本。
 * step_counter 跨 epoch 累计（调度用）；loss_out 接收本 epoch 的
 * 全批平均损失（m 次额外评估）。
 */
int sgd_epoch(const sgd_problem *p, double *w, sgd_state *st,
              const double *X, const double *Y, int m, int dim,
              int batch, const sgd_lr_sched *sched, int kind, double wd,
              mlab_rng *rng, long *step_counter, double *loss_out);

/* 全批平均损失 */
double sgd_full_loss(const sgd_problem *p, const double *w, const double *X,
                     const double *Y, int m, int dim);

/*
 * SVRG（路线图 L304 概览级实现）：每 epoch 先算 w̃ 处全梯度 μ，再按洗牌
 * 顺序逐样本更新 g̃_i = ∇ℓ_i(w) − ∇ℓ_i(w̃) + μ。loss_hist 记录每 epoch
 * 结束时的全批损失（可空）。返回 0。
 */
int mlab_svrg(const sgd_problem *p, double *w, const double *X, const double *Y,
              int m, int dim, int epochs, double lr, mlab_rng *rng,
              double *loss_hist, int hist_cap, int *hist_len);

#endif /* sgd_H */
