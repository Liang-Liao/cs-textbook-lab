#include "tpe.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_tpe_init(mlab_tpe *tp, double lb, double ub, double gamma, int cap)
{
    if (!tp || !(ub > lb) || cap < 4) return -1;
    memset(tp, 0, sizeof *tp);
    tp->lb = lb;
    tp->ub = ub;
    tp->gamma = gamma > 0.0 && gamma < 1.0 ? gamma : 0.25;
    tp->cap = cap;
    tp->x = (double *)malloc((size_t)cap * sizeof(double));
    tp->y = (double *)malloc((size_t)cap * sizeof(double));
    if (!tp->x || !tp->y) {
        mlab_tpe_free(tp);
        return -1;
    }
    return 0;
}

void mlab_tpe_free(mlab_tpe *tp)
{
    if (!tp) return;
    free(tp->x);
    free(tp->y);
    tp->x = tp->y = NULL;
    tp->n = tp->cap = 0;
}

int mlab_tpe_add(mlab_tpe *tp, double x, double y)
{
    if (!tp || tp->n >= tp->cap) return -1;
    if (x < tp->lb) x = tp->lb;
    if (x > tp->ub) x = tp->ub;
    tp->x[tp->n] = x;
    tp->y[tp->n] = y;
    ++tp->n;
    return 0;
}

double mlab_tpe_kde(const double *xs, int n, double bw, double x)
{
    double s = 0.0;
    int i;
    if (n <= 0 || !(bw > 0.0)) return 0.0;
    for (i = 0; i < n; ++i) {
        double u = (x - xs[i]) / bw;
        s += exp(-0.5 * u * u);
    }
    return s / ((double)n * bw * 2.5066282746310002);
}

double mlab_tpe_silverman(const double *xs, int n, double lb, double ub)
{
    double mean = 0.0, var = 0.0, sd, *tmp, med, iqr, bw, range;
    int i, j, k;
    range = ub - lb;
    if (n < 2) return range > 0 ? 0.25 * range : 1.0; /* 单点：核不能覆盖全盒 */
    for (i = 0; i < n; ++i) mean += xs[i];
    mean /= (double)n;
    for (i = 0; i < n; ++i) var += (xs[i] - mean) * (xs[i] - mean);
    sd = sqrt(var / (double)(n - 1));
    /* IQR：排序副本上的分位差 */
    tmp = (double *)malloc((size_t)n * sizeof(double));
    if (!tmp) return sd > 0 ? sd : (ub - lb);
    memcpy(tmp, xs, (size_t)n * sizeof(double));
    for (i = 1; i < n; ++i) {
        double v = tmp[i];
        for (j = i - 1; j >= 0 && tmp[j] > v; --j) tmp[j + 1] = tmp[j];
        tmp[j + 1] = v;
    }
    k = (int)(0.75 * (n - 1));
    med = tmp[k];
    k = (int)(0.25 * (n - 1));
    iqr = med - tmp[k];
    free(tmp);
    bw = 0.9 * fmin(sd, iqr > 0 ? iqr / 1.34 : sd) * pow((double)n, -0.2);
    if (!(bw > 1e-3 * range)) bw = 1e-3 * range;
    if (bw > 0.25 * range) bw = 0.25 * range; /* 封顶：防止 l 退化为全盒平坦 */
    return bw;
}

double mlab_tpe_propose(mlab_tpe *tp, mlab_rng *rng, int n_cand,
                        double *score_out)
{
    int ng, nb, i, c, nc;
    double *xs_good, *xs_bad, *idx;
    double bwg, bwb, best_score, best_x;
    /* 观测不足：均匀随机 */
    if (!tp || !rng || tp->n < 4) {
        double x = tp ? tp->lb + (tp->ub - tp->lb) * mlab_rng_uniform(rng) : 0.0;
        if (score_out) *score_out = 0.0;
        return x;
    }
    ng = (int)(tp->gamma * (double)tp->n);
    if (ng < 1) ng = 1;
    if (ng > tp->n - 1) ng = tp->n - 1;
    nb = tp->n - ng;
    xs_good = (double *)malloc((size_t)ng * sizeof(double));
    xs_bad = (double *)malloc((size_t)nb * sizeof(double));
    idx = (double *)malloc((size_t)tp->n * sizeof(double));
    if (!xs_good || !xs_bad || !idx) {
        free(xs_good);
        free(xs_bad);
        free(idx);
        if (score_out) *score_out = 0.0;
        return tp->lb + (tp->ub - tp->lb) * mlab_rng_uniform(rng);
    }
    /* 按目标值升序取前 ng 为 good（最小化） */
    for (i = 0; i < tp->n; ++i) idx[i] = (double)i;
    for (i = 1; i < tp->n; ++i) {
        double vi = idx[i];
        int j = i - 1;
        while (j >= 0 && tp->y[(int)idx[j]] > tp->y[(int)vi]) {
            idx[j + 1] = idx[j];
            --j;
        }
        idx[j + 1] = vi;
    }
    for (i = 0; i < ng; ++i) xs_good[i] = tp->x[(int)idx[i]];
    for (i = 0; i < nb; ++i) xs_bad[i] = tp->x[(int)idx[ng + i]];
    bwg = mlab_tpe_silverman(xs_good, ng, tp->lb, tp->ub);
    bwb = mlab_tpe_silverman(xs_bad, nb, tp->lb, tp->ub);

    nc = n_cand < 1 ? 24 : n_cand;
    best_score = -1e300;
    best_x = tp->lb;
    {
        int tries = 0, have = 0;
        for (c = 0; c < nc; ++c) {
            /* 候选从 l(x) 采样：随机 good 点 + 高斯扰动（截断到盒内）。
             * 拒绝与已评估点重合的候选：防边界堆积与重复提议。 */
            double xg = xs_good[(int)(mlab_rng_uniform(rng) * (double)ng)];
            double xc = xg + bwg * mlab_rng_normal(rng);
            double lg, lp, score;
            int dup = 0;
            if (xc < tp->lb) xc = tp->lb;
            if (xc > tp->ub) xc = tp->ub;
            for (i = 0; i < tp->n; ++i) {
                if (fabs(tp->x[i] - xc) < 1e-9 * (tp->ub - tp->lb)) {
                    dup = 1;
                    break;
                }
            }
            if (dup) {
                --c;
                if (++tries > 4 * nc) break;
                continue;
            }
            lg = mlab_tpe_kde(xs_good, ng, bwg, xc);
            lp = mlab_tpe_kde(xs_bad, nb, bwb, xc);
            score = log(fmax(lg, 1e-300)) - log(fmax(lp, 1e-300));
            have = 1;
            if (score > best_score) {
                best_score = score;
                best_x = xc;
            }
        }
        if (!have) {
            /* 候选全部重合（极端堆积）：均匀随机探索一步 */
            best_x = tp->lb + (tp->ub - tp->lb) * mlab_rng_uniform(rng);
            best_score = 0.0;
        }
    }
    free(xs_good);
    free(xs_bad);
    free(idx);
    if (score_out) *score_out = best_score;
    return best_x;
}
