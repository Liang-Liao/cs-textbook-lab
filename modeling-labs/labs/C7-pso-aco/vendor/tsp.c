#include "tsp.h"
#include "sa.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_tsp_random(mlab_rng *rng, mlab_tsp_inst *inst, int n, double box)
{
    int i;
    if (!rng || !inst || n < 3 || box <= 0) return 0;
    memset(inst, 0, sizeof *inst);
    inst->n = n;
    inst->x = (double *)malloc((size_t)n * sizeof(double));
    inst->y = (double *)malloc((size_t)n * sizeof(double));
    if (!inst->x || !inst->y) {
        mlab_tsp_free(inst);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        inst->x[i] = mlab_rng_uniform(rng) * box;
        inst->y[i] = mlab_rng_uniform(rng) * box;
    }
    return 1;
}

int mlab_tsp_random_matrix(mlab_rng *rng, mlab_tsp_inst *inst, int n,
                           double dmin, double dmax)
{
    int i, j;
    if (!rng || !inst || n < 3 || !(dmax > dmin)) return 0;
    memset(inst, 0, sizeof *inst);
    inst->n = n;
    inst->dist = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!inst->dist) {
        mlab_tsp_free(inst);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        inst->dist[i * n + i] = 0.0;
        for (j = i + 1; j < n; ++j) {
            double d = dmin + (dmax - dmin) * mlab_rng_uniform(rng);
            inst->dist[i * n + j] = d;
            inst->dist[j * n + i] = d;
        }
    }
    return 1;
}

void mlab_tsp_free(mlab_tsp_inst *inst)
{
    if (!inst) return;
    free(inst->x);
    free(inst->y);
    free(inst->dist);
    inst->x = inst->y = inst->dist = NULL;
    inst->n = 0;
}

double mlab_tsp_length(const mlab_tsp_inst *inst, const int *tour)
{
    double s = 0.0;
    int i, n;
    if (!inst || !tour || inst->n < 2) return 0.0;
    n = inst->n;
    if (inst->dist) {
        for (i = 0; i < n; ++i) {
            int a = tour[i];
            int b = tour[(i + 1) % n];
            s += inst->dist[a * n + b];
        }
        return s;
    }
    for (i = 0; i < n; ++i) {
        int a = tour[i];
        int b = tour[(i + 1) % n];
        double dx = inst->x[a] - inst->x[b];
        double dy = inst->y[a] - inst->y[b];
        s += sqrt(dx * dx + dy * dy);
    }
    return s;
}

void mlab_tsp_identity_tour(int n, int *tour)
{
    int i;
    for (i = 0; i < n; ++i) tour[i] = i;
}

void mlab_tsp_shuffle_tour(mlab_rng *rng, int n, int *tour)
{
    int i;
    mlab_tsp_identity_tour(n, tour);
    for (i = n - 1; i > 0; --i) {
        int j = (int)(mlab_rng_uniform(rng) * (i + 1));
        int t;
        if (j < 0) j = 0;
        if (j > i) j = i;
        t = tour[i];
        tour[i] = tour[j];
        tour[j] = t;
    }
}

void mlab_tsp_apply_swap(int *tour, int n, int i, int j)
{
    int t;
    if (!tour || n < 2) return;
    if (i < 0 || j < 0 || i >= n || j >= n || i == j) return;
    t = tour[i];
    tour[i] = tour[j];
    tour[j] = t;
}

void mlab_tsp_apply_2opt(int *tour, int n, int i, int j)
{
    int a, b;
    if (!tour || n < 2) return;
    if (i < 0 || j < 0 || i >= n || j >= n || i == j) return;
    if (i > j) {
        int t = i;
        i = j;
        j = t;
    }
    /* 翻转 tour[i+1 .. j] */
    a = i + 1;
    b = j;
    while (a < b) {
        int t = tour[a];
        tour[a] = tour[b];
        tour[b] = t;
        ++a;
        --b;
    }
}

void mlab_tsp_apply_oropt(int *tour, int n, int i, int j)
{
    int city, k;
    if (!tour || n < 3) return;
    if (i < 0 || j < 0 || i >= n || j >= n || i == j) return;
    city = tour[i];
    if (i < j) {
        for (k = i; k < j; ++k) tour[k] = tour[k + 1];
        tour[j] = city;
    } else {
        for (k = i; k > j; --k) tour[k] = tour[k - 1];
        tour[j] = city;
    }
}

void mlab_tsp_sa_result_free(mlab_tsp_sa_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

int mlab_sa_tsp(mlab_rng *rng,
                const mlab_tsp_inst *inst,
                const mlab_tsp_sa_config *cfg,
                const int *tour0,
                mlab_tsp_sa_result *out)
{
    int n, *cur, *best, *cand;
    double T, T0, f_cur, f_best;
    int outer, max_outer;
    long n_prop = 0, n_acc = 0;

    if (!rng || !inst || !cfg || !tour0 || !out || inst->n < 3) return 0;
    if (cfg->alpha <= 0 || cfg->alpha >= 1 || cfg->inner <= 0) return 0;
    n = inst->n;
    memset(out, 0, sizeof *out);
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    cand = (int *)malloc((size_t)n * sizeof(int));
    if (!cur || !best || !cand) {
        free(cur);
        free(best);
        free(cand);
        return 0;
    }
    memcpy(cur, tour0, (size_t)n * sizeof(int));
    memcpy(best, tour0, (size_t)n * sizeof(int));
    f_cur = mlab_tsp_length(inst, cur);
    f_best = f_cur;
    out->best_tour = best;
    T0 = cfg->T0 > 0 ? cfg->T0 : 1.0;
    max_outer = cfg->max_outer;
    if (max_outer <= 0) {
        double Tmin = cfg->T_min > 0 ? cfg->T_min : 1e-3;
        max_outer = (int)(log(Tmin / T0) / log(cfg->alpha)) + 2;
        if (max_outer < 10) max_outer = 10;
        if (max_outer > 20000) max_outer = 20000;
    }
    T = T0;
    for (outer = 0; outer < max_outer; ++outer) {
        double Tmin = cfg->T_min > 0 ? cfg->T_min : 1e-3;
        int k;
        if (T <= Tmin) break;
        for (k = 0; k < cfg->inner; ++k) {
            int i, j, ok = 0;
            double fy;
            /* 随机邻域提议（重试少量次数避免 i==j 与 2-opt 恒等移动） */
            int tries;
            for (tries = 0; tries < 8 && !ok; ++tries) {
                i = (int)(mlab_rng_uniform(rng) * n);
                j = (int)(mlab_rng_uniform(rng) * n);
                if (i < 0) i = 0;
                if (j < 0) j = 0;
                if (i >= n) i = n - 1;
                if (j >= n) j = n - 1;
                if (i == j) continue;
                if (cfg->nbhd == MLAB_TSP_NBHD_2OPT) {
                    int lo = i < j ? i : j, hi = i < j ? j : i;
                    /* 翻转 tour[lo+1..hi]：hi==lo+1 为恒等；
                     * (0,n-1) 为整段逆向，对称 TSP 下长度不变——都跳过 */
                    if (hi == lo + 1 || (lo == 0 && hi == n - 1)) continue;
                }
                memcpy(cand, cur, (size_t)n * sizeof(int));
                if (cfg->nbhd == MLAB_TSP_NBHD_2OPT)
                    mlab_tsp_apply_2opt(cand, n, i, j);
                else
                    mlab_tsp_apply_swap(cand, n, i, j);
                ok = 1;
            }
            if (!ok) continue;
            fy = mlab_tsp_length(inst, cand);
            ++n_prop;
            ++out->n_fevals;
            if (mlab_sa_metropolis(fy - f_cur, T, rng)) {
                memcpy(cur, cand, (size_t)n * sizeof(int));
                f_cur = fy;
                ++n_acc;
                if (f_cur < f_best) {
                    f_best = f_cur;
                    memcpy(best, cur, (size_t)n * sizeof(int));
                }
            }
        }
        T *= cfg->alpha;
        ++out->n_outer;
    }
    out->best_len = f_best;
    out->final_len = f_cur;
    out->n_accept = (int)n_acc;
    out->n_fevals = (int)n_prop;
    out->accept_rate = n_prop > 0 ? (double)n_acc / (double)n_prop : 0.0;
    free(cur);
    free(cand);
    return 1;
}
