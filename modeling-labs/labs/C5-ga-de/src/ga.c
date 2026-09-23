#include "ga.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_ga_result_free(mlab_ga_result *r)
{
    if (!r) return;
    free(r->best_bits);
    free(r->div_hist);
    r->best_bits = NULL;
    r->div_hist = NULL;
}

double mlab_ga_bitstring_fit(const unsigned char *bits, int n, int deceptive)
{
    int i, ones = 0;
    if (!bits || n <= 0) return 0.0;
    for (i = 0; i < n; ++i) ones += bits[i] ? 1 : 0;
    if (!deceptive) return (double)ones;
    /* 陷阱：全局全1最优；次优在全0 */
    if (ones == n) return (double)n;
    return (double)(n - 1) - (double)ones;
}

double mlab_ga_pop_diversity(const unsigned char *pop, int npop, int bits)
{
    int b, i;
    double sum_h = 0.0;
    if (!pop || npop < 2 || bits <= 0) return 0.0;
    for (b = 0; b < bits; ++b) {
        int ones = 0;
        double p, h;
        for (i = 0; i < npop; ++i)
            if (pop[i * bits + b]) ++ones;
        p = (double)ones / npop;
        if (p <= 0.0 || p >= 1.0)
            h = 0.0;
        else
            h = -(p * log(p) + (1.0 - p) * log(1.0 - p));
        sum_h += h / log(2.0);
    }
    return sum_h / bits;
}

static int tournament(mlab_rng *rng, const double *fit, int npop, int k)
{
    int best = (int)(mlab_rng_uniform(rng) * npop) % npop;
    int i;
    if (best < 0) best = 0;
    for (i = 1; i < k; ++i) {
        int c = (int)(mlab_rng_uniform(rng) * npop) % npop;
        if (c < 0) c = 0;
        if (fit[c] > fit[best]) best = c;
    }
    return best;
}

static int roulette(mlab_rng *rng, const double *fit, int npop)
{
    double sum = 0.0, u, acc = 0.0;
    int i;
    for (i = 0; i < npop; ++i) sum += fit[i] + 1e-12;
    u = mlab_rng_uniform(rng) * sum;
    for (i = 0; i < npop; ++i) {
        acc += fit[i] + 1e-12;
        if (u <= acc) return i;
    }
    return npop - 1;
}

int mlab_ga_run(mlab_rng *rng, const mlab_ga_config *cfg, mlab_ga_result *out,
                double low_thresh)
{
    int bits, npop, g, i, b;
    unsigned char *pop, *next, *used;
    double *fit;
    int max_gen, k;
    int div_cap;

    if (!rng || !cfg || !out || cfg->bits < 2 || cfg->pop < 2) return 0;
    bits = cfg->bits;
    npop = cfg->pop;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 100;
    k = cfg->tour_k > 0 ? cfg->tour_k : 3;
    memset(out, 0, sizeof *out);
    pop = (unsigned char *)malloc((size_t)npop * bits);
    next = (unsigned char *)malloc((size_t)npop * bits);
    used = (unsigned char *)calloc((size_t)npop, 1);
    fit = (double *)malloc((size_t)npop * sizeof(double));
    div_cap = max_gen + 2;
    out->div_hist = (double *)malloc((size_t)div_cap * sizeof(double));
    out->best_bits = (int *)malloc((size_t)bits * sizeof(int));
    out->first_low_gen = -1;
    if (!pop || !next || !used || !fit || !out->div_hist || !out->best_bits) {
        free(pop); free(next); free(used); free(fit);
        mlab_ga_result_free(out);
        return 0;
    }
    for (i = 0; i < npop; ++i)
        for (b = 0; b < bits; ++b)
            pop[i * bits + b] = mlab_rng_uniform(rng) < 0.5 ? 1 : 0;

    out->best_fit = -1e300;
    for (g = 0; g <= max_gen; ++g) {
        double div = mlab_ga_pop_diversity(pop, npop, bits);
        double best_f = -1e300;
        int best_i = 0;
        out->div_hist[out->div_len++] = div;
        if (out->first_low_gen < 0 && div < low_thresh)
            out->first_low_gen = g;
        for (i = 0; i < npop; ++i) {
            fit[i] = mlab_ga_bitstring_fit(&pop[i * bits], bits, cfg->use_deceptive);
            ++out->n_eval;
            if (fit[i] > best_f) {
                best_f = fit[i];
                best_i = i;
            }
        }
        if (best_f > out->best_fit) {
            out->best_fit = best_f;
            for (b = 0; b < bits; ++b)
                out->best_bits[b] = pop[best_i * bits + b];
        }
        if (g == max_gen) break;

        /* 精英：按适应度取 k 个不同个体（真 top-k 去重），空位由子代填充 */
        {
            int n_el = cfg->elite > 0 ? cfg->elite : 0;
            if (n_el > npop) n_el = npop;
            memset(used, 0, (size_t)npop);
            for (i = 0; i < n_el; ++i) {
                int bi = -1, j;
                double bf = -1e300;
                for (j = 0; j < npop; ++j) {
                    if (!used[j] && fit[j] > bf) {
                        bf = fit[j];
                        bi = j;
                    }
                }
                if (bi < 0) break;
                used[bi] = 1;
                memcpy(&next[i * bits], &pop[bi * bits], (size_t)bits);
            }
            for (i = n_el; i < npop; ++i) {
                int p1 = (cfg->sel == MLAB_GA_SEL_TOURNAMENT)
                             ? tournament(rng, fit, npop, k)
                             : roulette(rng, fit, npop);
                int p2 = (cfg->sel == MLAB_GA_SEL_TOURNAMENT)
                             ? tournament(rng, fit, npop, k)
                             : roulette(rng, fit, npop);
                unsigned char *c = &next[i * bits];
                memcpy(c, &pop[p1 * bits], (size_t)bits);
                if (mlab_rng_uniform(rng) < cfg->p_cross) {
                    if (cfg->xover == MLAB_GA_X_1POINT) {
                        int cx = 1 + (int)(mlab_rng_uniform(rng) * (bits - 1));
                        if (cx >= bits) cx = bits - 1;
                        memcpy(c + cx, &pop[p2 * bits + cx], (size_t)(bits - cx));
                    } else {
                        int bb;
                        for (bb = 0; bb < bits; ++bb)
                            if (mlab_rng_uniform(rng) < 0.5)
                                c[bb] = pop[p2 * bits + bb];
                    }
                }
                for (b = 0; b < bits; ++b)
                    if (mlab_rng_uniform(rng) < cfg->p_mut)
                        c[b] = 1 - c[b];
            }
        }
        memcpy(pop, next, (size_t)npop * bits);
        out->gen_used = g + 1;
    }
    free(pop); free(next); free(used); free(fit);
    return 1;
}
