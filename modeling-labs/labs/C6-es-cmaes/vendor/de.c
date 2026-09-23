#include "de.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_de_result_free(mlab_de_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->div_hist);
    r->best_x = NULL;
    r->div_hist = NULL;
}

double mlab_de_pop_diversity(const double *pop, int pop_n, int dim)
{
    int i, j, d;
    double sum = 0.0;
    long pairs = 0;
    if (!pop || pop_n < 2 || dim <= 0) return 0.0;
    for (i = 0; i < pop_n; ++i) {
        for (j = i + 1; j < pop_n; ++j) {
            double s = 0.0;
            for (d = 0; d < dim; ++d) {
                double df = pop[i * dim + d] - pop[j * dim + d];
                s += df * df;
            }
            sum += sqrt(s);
            ++pairs;
        }
    }
    return pairs > 0 ? sum / pairs : 0.0;
}

static void clip_box(double *x, int dim, const double *lb, const double *ub)
{
    int d;
    if (!lb || !ub) return;
    for (d = 0; d < dim; ++d) {
        if (x[d] < lb[d]) x[d] = lb[d];
        if (x[d] > ub[d]) x[d] = ub[d];
    }
}

int mlab_de_run(mlab_rng *rng, const mlab_de_config *cfg,
                const double *x0_pop,
                mlab_de_result *out,
                double low_thresh)
{
    int dim, npop, g, i, d;
    double *pop, *trial, *fit;
    double F, CR;
    int max_gen;

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0 || cfg->pop < 4)
        return 0;
    dim = cfg->dim;
    npop = cfg->pop;
    F = cfg->F;
    CR = cfg->CR;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 100;
    memset(out, 0, sizeof *out);
    pop = (double *)malloc((size_t)npop * dim * sizeof(double));
    trial = (double *)malloc((size_t)dim * sizeof(double));
    fit = (double *)malloc((size_t)npop * sizeof(double));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->div_hist = (double *)malloc((size_t)(max_gen + 2) * sizeof(double));
    out->first_low_gen = -1;
    if (!pop || !trial || !fit || !out->best_x || !out->div_hist) {
        free(pop); free(trial); free(fit);
        mlab_de_result_free(out);
        return 0;
    }
    if (x0_pop) {
        memcpy(pop, x0_pop, (size_t)npop * dim * sizeof(double));
    } else if (cfg->lb && cfg->ub) {
        for (i = 0; i < npop; ++i)
            for (d = 0; d < dim; ++d)
                pop[i * dim + d] = cfg->lb[d] +
                    (cfg->ub[d] - cfg->lb[d]) * mlab_rng_uniform(rng);
    } else {
        for (i = 0; i < npop * dim; ++i)
            pop[i] = mlab_rng_normal(rng);
    }

    out->best_f = 1e300;
    for (g = 0; g <= max_gen; ++g) {
        double div = mlab_de_pop_diversity(pop, npop, dim);
        double rel;
        int best_i = 0;
        if (g == 0) out->div0 = div > 0 ? div : 1.0;
        rel = div / out->div0;
        out->div_hist[out->div_len++] = rel; /* 相对多样性 */
        if (out->first_low_gen < 0 && rel < low_thresh)
            out->first_low_gen = g;
        /* 父代适应度只在初始代评估一次，此后由选择步同步维护 fit[]，
         * 每代只评 npop 个 trial；best_i 从缓存 fit 取 argmin（无评估） */
        if (g == 0) {
            for (i = 0; i < npop; ++i) {
                fit[i] = cfg->f(&pop[i * dim], dim, cfg->ctx);
                ++out->n_eval;
            }
        }
        for (i = 1; i < npop; ++i)
            if (fit[i] < fit[best_i]) best_i = i;
        if (fit[best_i] < out->best_f) {
            out->best_f = fit[best_i];
            memcpy(out->best_x, &pop[best_i * dim], (size_t)dim * sizeof(double));
        }
        if (g == max_gen) break;

        for (i = 0; i < npop; ++i) {
            int r1, r2, r3, jrand;
            do { r1 = (int)(mlab_rng_uniform(rng) * npop) % npop; } while (r1 == i);
            do { r2 = (int)(mlab_rng_uniform(rng) * npop) % npop; } while (r2 == i || r2 == r1);
            do { r3 = (int)(mlab_rng_uniform(rng) * npop) % npop; } while (r3 == i || r3 == r1 || r3 == r2);
            jrand = (int)(mlab_rng_uniform(rng) * dim) % dim;
            for (d = 0; d < dim; ++d) {
                if (cfg->variant == MLAB_DE_BEST1)
                    trial[d] = pop[best_i * dim + d] +
                               F * (pop[r1 * dim + d] - pop[r2 * dim + d]);
                else if (cfg->variant == MLAB_DE_CUR2BEST1)
                    trial[d] = pop[i * dim + d] +
                               F * (pop[best_i * dim + d] - pop[i * dim + d]) +
                               F * (pop[r1 * dim + d] - pop[r2 * dim + d]);
                else /* rand/1 */
                    trial[d] = pop[r1 * dim + d] +
                               F * (pop[r2 * dim + d] - pop[r3 * dim + d]);
                if (!(mlab_rng_uniform(rng) < CR) && d != jrand)
                    trial[d] = pop[i * dim + d];
            }
            clip_box(trial, dim, cfg->lb, cfg->ub);
            {
                double ft = cfg->f(trial, dim, cfg->ctx);
                ++out->n_eval;
                if (ft <= fit[i]) {
                    memcpy(&pop[i * dim], trial, (size_t)dim * sizeof(double));
                    fit[i] = ft;
                    if (ft < out->best_f) {
                        out->best_f = ft;
                        memcpy(out->best_x, trial, (size_t)dim * sizeof(double));
                    }
                }
            }
        }
        out->gen_used = g + 1;
    }
    free(pop); free(trial); free(fit);
    return 1;
}
