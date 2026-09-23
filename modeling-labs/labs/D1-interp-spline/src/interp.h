#ifndef MLAB_INTERP_H
#define MLAB_INTERP_H

/* D1 插值与样条：多项式 / 分段 / 自然三次样条 / RBF */

/* ---- 多项式插值 ---- */
double mlab_lagrange_eval(const double *xn, const double *yn, int n, double x);

/* Newton 差商：coef[i] = f[x0..xi] */
int mlab_newton_divided_diff(const double *xn, const double *yn, int n, double *coef);
double mlab_newton_eval(const double *xn, const double *coef, int n, double x);

/* ---- 分段 ---- */
double mlab_piecewise_linear_eval(const double *xn, const double *yn, int n, double xq);
/* 三次 Hermite：节点值 + 节点一阶导 */
double mlab_cubic_hermite_eval(const double *xn, const double *yn, const double *yp,
                               int n, double xq);

/* ---- 自然三次样条（M0 = M_{n-1} = 0） ---- */
typedef struct {
    int n;
    double *x;
    double *y;
    double *M; /* 二阶导，长度 n */
} mlab_cspline;

int mlab_cspline_fit(mlab_cspline *sp, const double *xn, const double *yn, int n);
double mlab_cspline_eval(const mlab_cspline *sp, double xq);
void mlab_cspline_free(mlab_cspline *sp);

/* Thomas 三对角：a 次对角(a[0]不用)、b 对角、c 超对角(c[n-1]不用) */
int mlab_thomas(int n, const double *a, const double *b, const double *c,
                const double *d, double *x);

/* ---- RBF ---- */
enum {
    MLAB_RBF_GAUSSIAN = 0,
    MLAB_RBF_TPS = 1
};

typedef struct {
    int n;
    int dim;
    int kind;
    double shape; /* Gaussian: ε in φ(r)=exp(-(ε r)^2) */
    double *A;    /* n×n 核矩阵（行主序）；TPS 时为增广前的核块 */
    double *alpha;
    double *xs;   /* n×dim 中心 */
    double *ys;
    /* TPS 多项式增广（degree-1：[1, x1, ..., xdim]）。
     * 薄板样条核仅为条件正定，需多项式增广项保证唯一可解，
     * 且插值体可精确再生一次多项式。 */
    int poly;     /* 1: 已增广 */
    int m;        /* 增广项数 = dim+1 */
    double *pc;   /* m 个多项式系数 */
} mlab_rbf;

double mlab_rbf_kernel(int kind, double shape, double r);
/* 拟合：重复节点（距离 < 1e-9·尺度）返回 -2；线性系统失败返回 -1 */
int mlab_rbf_fit(mlab_rbf *rbf, const double *xs, const double *ys,
                 int n, int dim, int kind, double shape);
double mlab_rbf_eval(const mlab_rbf *rbf, const double *x);
void mlab_rbf_free(mlab_rbf *rbf);
/* 核矩阵条件数估计（A1）；失败返回负数 */
double mlab_rbf_cond2(const mlab_rbf *rbf);

/* ---- 1D kriging（简单克里金；与 C9 GP 同源：同一核矩阵系统） ---- */
typedef struct {
    int n;
    double *x;    /* n 节点 */
    double *w;    /* n 克里金权重（K+nugget·I）⁻¹(y-ȳ) */
    double ls;    /* 高斯协方差长度尺度 */
    double sf2;   /* 信号方差 */
    double nugget;/* σn²（=0 为精确插值） */
    double ymean;
} mlab_kriging1d;

int mlab_kriging1d_fit(mlab_kriging1d *k, const double *x, const double *y,
                       int n, double ls, double sf2, double nugget);
double mlab_kriging1d_eval(const mlab_kriging1d *k, double xq);
void mlab_kriging1d_free(mlab_kriging1d *k);

/* ---- 工具 ---- */
void mlab_write_pgm_grid(const char *path, const double *val, int ncol, int nrow);

#endif /* MLAB_INTERP_H */
