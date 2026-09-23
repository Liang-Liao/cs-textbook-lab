#ifndef MLAB_TPE_H
#define MLAB_TPE_H

#include "rng.h"

/*
 * C9 地图项落地：TPE（Tree-structured Parzen Estimator）简化版。
 * 仅 1D 输入空间（路线图 :571 为地图级概览，此处给最小可复算实现）：
 *   - 观测按目标值升序切 good（前 γ 比例）/ bad 两组；
 *   - l(x)、g(x) 分别为两组观测上的高斯 KDE（1D 分段核：每个观测点放一个核）；
 *   - 建议点：从 l(x) 采样 n_cand 个候选，取 log l(x) - log g(x) 最大者。
 * 参考 Bergstra et al. 2011（NeurIPS），带宽 Silverman 规则。
 */

typedef struct {
    double lb, ub;
    double gamma;  /* good 分位比例（最小化：前 gamma·n 个观测为 good） */
    double *x, *y; /* 观测（按加入顺序） */
    int n, cap;
} mlab_tpe;

/* cap = 预算上限；gamma<=0 取 0.25 */
int mlab_tpe_init(mlab_tpe *tp, double lb, double ub, double gamma, int cap);
void mlab_tpe_free(mlab_tpe *tp);
/* 加入一个观测（最小化问题）；满员返回 -1 */
int mlab_tpe_add(mlab_tpe *tp, double x, double y);

/* 1D 高斯 KDE（等权，带宽 bw）；xs 无序，n 点 */
double mlab_tpe_kde(const double *xs, int n, double bw, double x);

/* Silverman 带宽：0.9·min(σ, IQR/1.34)·n^(-1/5)，盒宽下限保护 */
double mlab_tpe_silverman(const double *xs, int n, double lb, double ub);

/*
 * 提出下一个建议点（最小化）。
 * 观测 <4 个时退化为盒内均匀随机。
 * n_cand：从 l(x) 采样的候选数（<1 取 24）。
 * score_out 可空，返回 log l - log g（越大越值得试）。
 */
double mlab_tpe_propose(mlab_tpe *tp, mlab_rng *rng, int n_cand,
                        double *score_out);

#endif /* MLAB_TPE_H */
