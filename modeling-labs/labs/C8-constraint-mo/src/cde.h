#ifndef MLAB_CDE_H
#define MLAB_CDE_H

#include "rng.h"

/* C8 约束 DE：死亡惩罚 vs Deb 可行性法则 */

typedef enum {
    MLAB_CDE_DEATH = 0,      /* 不可行 → 大惩罚适应度 */
    MLAB_CDE_DEB = 1,        /* Deb 2000：可行优先；不可行比违反度 */
    MLAB_CDE_STATIC_PEN = 2, /* 静态罚：score = f + μ·viol（μ = penalty_mu） */
    MLAB_CDE_ADAPT_PEN = 3   /* 自适应罚：μ 按上一代可行比例乘性调整 */
} mlab_cde_mode;

/* 不等式 g(x)<=0，写入 g[0..m-u-1] */
typedef void (*mlab_cde_ineq)(const double *x, int n, double *g, int m, void *ctx);

/* 修复算子：就地修复 x（如投影到可行域）；可空 */
typedef void (*mlab_cde_repair)(double *x, int n, void *ctx);

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    mlab_cde_ineq ineq;
    void *ineq_ctx;
    int m_ineq;
    int dim;
    const double *lb, *ub;
    int pop;
    int max_gen;
    double F, CR;
    mlab_cde_mode mode;
    double death_penalty;   /* 死亡惩罚基数，默认 1e20 */
    double penalty_mu;      /* 静态罚系数（STATIC_PEN 用，默认 1.0）；
                               ADAPT_PEN 为初始 μ */
    double adapt_mu_factor; /* 自适应乘性因子 β（默认 2.0） */
    mlab_cde_repair repair; /* 修复算子（可空） */
    void *repair_ctx;
} mlab_cde_config;

typedef struct {
    double best_f;       /* 最优可行解的 f；若无可行解为代表解的 f */
    double best_viol;    /* 该解（或最终代表）的约束违反度 */
    int feasible;        /* 是否找到可行解 */
    double *best_x;
    int n_eval;
    int gen_used;
    double viol_final;   /* 末代最优违反度（不可行时仍记录） */
    double viol_min;     /* 全程见过的最小违反度（死亡惩罚无梯度的对照信号） */
    double mu_final;     /* 自适应罚的末代 μ（其他模式 = penalty_mu） */
    int n_feas_final;    /* 末代可行个体数 */
} mlab_cde_result;

void mlab_cde_result_free(mlab_cde_result *r);

/* 约束违反度：sum max(0,g_i) */
double mlab_cde_violation(const double *x, int n, mlab_cde_ineq ineq,
                          int m, void *ctx);

/* Deb 比较：返回 1 表示 a 优于 b（用于 DE 选择） */
int mlab_deb_better(double fa, double va, double fb, double vb);

int mlab_cde_run(mlab_rng *rng, const mlab_cde_config *cfg, mlab_cde_result *out);

/* 内置测试问题：5D 球约束 — min Σ(x_i-1.5)^2 s.t. ||x||²≤r²，r 可配 */
double mlab_prob_disk_f(const double *x, int n, void *ctx);
void mlab_prob_disk_g(const double *x, int n, double *g, int m, void *ctx);
/* f* 在 ||x||=r 的球面上；ctx 为 double* 半径，空则 r=1 */
double mlab_prob_disk_fstar(int n, double r);
/* 搜索盒半宽 */
double mlab_prob_disk_box(double r);
/* 球约束修复（投影）：||x||>r 时缩放回球面；ctx 为 double* 半径，空则 r=1 */
void mlab_prob_disk_repair(double *x, int n, void *ctx);

#endif
