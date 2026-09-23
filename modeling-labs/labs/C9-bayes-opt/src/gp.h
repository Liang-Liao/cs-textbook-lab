#ifndef MLAB_GP_H
#define MLAB_GP_H

/* C9 高斯过程回归：RBF 核 + Cholesky（A1） */

typedef struct {
    int dim;
    int n;            /* 训练点数 */
    const double *X;  /* n × dim 行主序 */
    const double *y;  /* n */
    double ls;        /* 长度尺度 ℓ */
    double sf2;       /* 信号方差 σ_f² */
    double noise;     /* 噪声方差 σ_n² */
    /* fit 后内部缓存 */
    double *L;        /* n × n 下三角 Cholesky */
    double *alpha;    /* n，K^{-1} y */
    double ymean;
    int fitted;
} mlab_gp;

void mlab_gp_init(mlab_gp *gp, int dim, const double *X, const double *y, int n,
                  double ls, double sf2, double noise);
void mlab_gp_release(mlab_gp *gp);

double mlab_gp_rbf(const double *a, const double *b, int dim,
                   double ls, double sf2);

/* 构造 K+σ_n²I 并 Cholesky 分解；失败返回 0 */
int mlab_gp_fit(mlab_gp *gp);

/* 后验：预测均值与方差（含噪声项） */
int mlab_gp_predict(const mlab_gp *gp, const double *x,
                    double *mean, double *var);

/* 采集函数（最小化）：EI / PI / UCB */
double mlab_acq_ei(double mean, double var, double best, double xi);
double mlab_acq_pi(double mean, double var, double best, double xi);
double mlab_acq_ucb(double mean, double var, double kappa); /* 最小化：μ - κσ */

#endif
