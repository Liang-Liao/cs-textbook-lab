#ifndef MLAB_RNG_H
#define MLAB_RNG_H

/*
 * C1 深化后的随机数基础设施。
 * 主发生器：splitmix64（与 A2 兼容）；对照：xorshift64* / LCG / MT19937。
 * 所有 lab 默认走 mlab_rng_seed → splitmix64，保证 A 层结果可复现。
 */

enum mlab_rng_kind {
    MLAB_RNG_SPLITMIX64 = 0,
    MLAB_RNG_XORSHIFT64STAR = 1,
    MLAB_RNG_LCG = 2,
    MLAB_RNG_MT19937 = 3
};

typedef struct {
    unsigned long long s;          /* splitmix64 / xorshift64* state */
    unsigned long long lcg_s;      /* LCG state */
    unsigned int mt[624];          /* MT19937 */
    int mt_idx;
    int kind;
    int normal_spare_valid;        /* Box-Muller 余样本 */
    double normal_spare;
} mlab_rng;

/* 默认 splitmix64（与历史 seed 行为一致） */
void mlab_rng_seed(mlab_rng *r, unsigned long long seed);
void mlab_rng_seed_kind(mlab_rng *r, unsigned long long seed, int kind);
const char *mlab_rng_kind_name(int kind);

/* 64-bit raw draw（MT 由两个 32-bit 拼成） */
unsigned long long mlab_rng_u64(mlab_rng *r);
/* U(0,1) 开区间，53-bit 精度 */
double mlab_rng_uniform(mlab_rng *r);

/* 正态：Box-Muller（默认）、Marsaglia 极法、逆变换 */
double mlab_rng_normal(mlab_rng *r);
double mlab_rng_normal_polar(mlab_rng *r);
double mlab_rng_normal_inv(mlab_rng *r);

double mlab_rng_exponential(mlab_rng *r, double lambda);
int mlab_rng_poisson(mlab_rng *r, double lambda);
/* 离散逆变换：Bernoulli(p) */
int mlab_rng_bernoulli(mlab_rng *r, double p);

/* ---- 质量测试（复用 A2 卡方；自相关 C1 自生长） ---- */
/* n 个均匀样本落入 k 等宽箱，返回 chi2；pval 可空 */
double mlab_rng_chi2_uniform(mlab_rng *r, int n, int k, double *pval);
/* 序列 x[0..n-1] 在 lag 处的样本自相关（除以总方差，有偏） */
double mlab_autocorr(const double *x, int n, int lag);
/* 生成 n 个均匀数，返回 lag=1..max_lag 上 max|rho|；argmax 可空 */
double mlab_rng_max_abs_acf(mlab_rng *r, int n, int max_lag, int *argmax_lag);

#endif /* MLAB_RNG_H */
