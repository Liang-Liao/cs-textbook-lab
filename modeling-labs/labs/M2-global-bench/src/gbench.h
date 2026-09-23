#ifndef MLAB_GBENCH_H
#define MLAB_GBENCH_H

#include "rng.h"

/* M2 统一基准：Sphere / Rosenbrock / Rastrigin / Griewank / Ackley */

enum {
    MLAB_GB_SPHERE = 0,
    MLAB_GB_ROSEN = 1,
    MLAB_GB_RASTRIGIN = 2,
    MLAB_GB_GRIEWANK = 3,
    MLAB_GB_ACKLEY = 4,
    MLAB_GB_NFUNCS = 5
};

enum {
    MLAB_GALGO_SA = 0,
    MLAB_GALGO_GA = 1,     /* 实数编码 GA */
    MLAB_GALGO_DE = 2,
    MLAB_GALGO_PSO = 3,
    MLAB_GALGO_CMAES = 4,
    MLAB_GALGO_HYBRID = 5, /* DE + Nelder-Mead 局部精修 */
    MLAB_GALGO_NALGOS = 6
};

typedef struct {
    int id;
    const char *name;
    double (*f)(const double *x, int n, void *ctx);
    double lb, ub;     /* 各维相同盒 */
    double fstar;      /* 全局最优值 */
    double target;     /* 成功判据：f <= target */
    int multimodal;
} mlab_gfunc;

const mlab_gfunc *mlab_gfunc_get(int id);
const char *mlab_galgo_name(int algo);

/* 单次运行结果 */
typedef struct {
    double best_f;
    int n_eval;
    int success;       /* best_f <= target */
    int evals_to_target; /* 成功时的评估数，否则 -1 */
    double *best_x;    /* dim，可空则不写 */
} mlab_galgo_out;

void mlab_galgo_out_free(mlab_galgo_out *o);

/*
 * 统一算法接口。budget ≈ 评估预算（种群/SA 按此折算代数/步数）。
 * 返回 0 成功运行，-1 失败。
 */
int mlab_galgo_run(int algo, mlab_rng *rng, int func_id, int dim, int budget,
                   mlab_galgo_out *out);

/* 简单实数编码 GA（M2 自生长，连续侧第四算法） */
int mlab_real_ga_run(mlab_rng *rng,
                     double (*f)(const double *, int, void *), void *ctx,
                     int dim, const double *lb, const double *ub,
                     int pop, int budget,
                     double *best_f, double *best_x, int *n_eval);

/* 混合：全局（默认 PSO）+ Hooke-Jeeves 模式搜索精修；总预算与单层同口径 */
int mlab_hybrid_run(mlab_rng *rng, int func_id, int dim, int budget,
                    int global_algo, mlab_galgo_out *out);

/* 两比例 z 检验（单侧 hybrid > single）：返回 p 值 */
double mlab_two_prop_p_one_sided(int s1, int n1, int s2, int n2);

#endif /* MLAB_GBENCH_H */
