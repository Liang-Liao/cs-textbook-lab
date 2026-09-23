#ifndef MLAB_DE_H
#define MLAB_DE_H

#include "rng.h"

/* C5 差分进化：rand/1、best/1（bin 交叉）；多样性轨迹 */

typedef enum {
    MLAB_DE_RAND1 = 0,
    MLAB_DE_BEST1 = 1,
    MLAB_DE_CUR2BEST1 = 2
} mlab_de_variant;

typedef struct {
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    int dim;
    const double *lb, *ub;
    int pop;
    int max_gen;     /* 代数；评估数≈ pop*(max_gen+1) */
    double F;        /* 缩放因子 */
    double CR;       /* 交叉率 */
    mlab_de_variant variant;
} mlab_de_config;

typedef struct {
    double best_f;
    double *best_x;
    int n_eval;
    int gen_used;
    double *div_hist; /* 每代相对多样性 div/div0 */
    double div0;      /* 初始平均两两距离 */
    int div_len;
    int first_low_gen; /* 相对多样性首次 < low_thresh 的代；未触发 -1 */
} mlab_de_result;

void mlab_de_result_free(mlab_de_result *r);

double mlab_de_pop_diversity(const double *pop, int pop_n, int dim);

int mlab_de_run(mlab_rng *rng, const mlab_de_config *cfg,
                const double *x0_pop, /* 可空→盒内均匀随机 init */
                mlab_de_result *out,
                double low_thresh);

#endif
