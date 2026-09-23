#ifndef MLAB_GA_H
#define MLAB_GA_H

#include "rng.h"

/* C5 二进制遗传算法：OneMax / 解耦函数；多样性诊断 */

typedef enum {
    MLAB_GA_SEL_ROULETTE = 0,
    MLAB_GA_SEL_TOURNAMENT = 1
} mlab_ga_sel;

typedef enum {
    MLAB_GA_X_1POINT = 0,
    MLAB_GA_X_UNIFORM = 1
} mlab_ga_xover;

typedef struct {
    int bits;           /* 染色体长度 */
    int pop;            /* 种群大小（偶数较方便） */
    int max_gen;
    double p_mut;       /* 每位变异概率 */
    double p_cross;     /* 交叉概率 */
    mlab_ga_sel sel;
    mlab_ga_xover xover;
    int tour_k;         /* 锦标赛规模，0→3 */
    int elite;          /* 精英保留个数 */
    int use_deceptive;  /* 0=OneMax 最大化 1；1=陷阱函数 */
} mlab_ga_config;

typedef struct {
    double best_fit;    /* 最大化适应度 */
    int *best_bits;     /* 长度 bits */
    int n_eval;
    int gen_used;
    double *div_hist;   /* 每代多样性 [0,1]，长度 gen_used+1（含初始） */
    int div_len;
    int first_low_gen;  /* 多样性首次 < low_thresh 的代数；未触发→-1 */
} mlab_ga_result;

void mlab_ga_result_free(mlab_ga_result *r);

/* 适应度：OneMax = 置位数；deceptive（长度 L，L>=2）:
 * 全 1 → L；否则  f = (L-1) - popcount  （局部最优在全 0 附近） */
double mlab_ga_bitstring_fit(const unsigned char *bits, int n, int deceptive);

/* 种群多样性：平均每位等位基因熵 / 0.693（或平均两两汉明/L/2） */
double mlab_ga_pop_diversity(const unsigned char *pop, int npop, int bits);

int mlab_ga_run(mlab_rng *rng, const mlab_ga_config *cfg, mlab_ga_result *out,
                double low_thresh);

#endif
