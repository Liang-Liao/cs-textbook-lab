#include "tabu.h"

#include <stdlib.h>
#include <string.h>

void mlab_tabu_result_free(mlab_tabu_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

static int pair_index(int i, int j, int n)
{
    if (i > j) {
        int t = i;
        i = j;
        j = t;
    }
    return i * n + j;
}

int mlab_tabu_tsp(mlab_rng *rng,
                  const mlab_tsp_inst *inst,
                  const mlab_tabu_config *cfg,
                  const int *tour0,
                  mlab_tabu_result *out)
{
    int n, *cur, *best, *cand;
    int *tabu_until;
    long *freq;
    double f_cur, f_best;
    int it, tenure;
    long fevals = 0;
    double lambda;

    if (!rng || !inst || !cfg || !tour0 || !out || inst->n < 4) return 0;
    if (cfg->tenure < 0) return 0;
    n = inst->n;
    tenure = cfg->tenure;
    lambda = cfg->freq_lambda > 0.0 ? cfg->freq_lambda : 0.0;
    memset(out, 0, sizeof *out);
    cur = (int *)malloc((size_t)n * sizeof(int));
    best = (int *)malloc((size_t)n * sizeof(int));
    cand = (int *)malloc((size_t)n * sizeof(int));
    tabu_until = (int *)calloc((size_t)n * (size_t)n + 1, sizeof(int));
    freq = (long *)calloc((size_t)n * (size_t)n + 1, sizeof(long));
    if (!cur || !best || !cand || !tabu_until || !freq) {
        free(cur); free(best); free(cand); free(tabu_until); free(freq);
        return 0;
    }
    memcpy(cur, tour0, (size_t)n * sizeof(int));
    memcpy(best, tour0, (size_t)n * sizeof(int));
    f_cur = mlab_tsp_length(inst, cur);
    f_best = f_cur;
    fevals = 1;
    out->best_tour = best;
    out->tenure_used = tenure;

    for (it = 1; it <= cfg->max_iter; ++it) {
        int bi = -1, bj = -1;
        double best_eff = 1e300, best_flen = 1e300;
        int best_tabu = 0, n_ties = 0;
        int i, j;

        if (cfg->max_fevals > 0 && fevals >= cfg->max_fevals) break;

        for (i = 0; i < n; ++i) {
            for (j = i + 1; j < n; ++j) {
                double flen, eff;
                int idx = pair_index(i, j, n);
                int is_tabu = tabu_until[idx] > it;
                int aspir;

                memcpy(cand, cur, (size_t)n * sizeof(int));
                if (cfg->use_2opt)
                    mlab_tsp_apply_2opt(cand, n, i, j);
                else
                    mlab_tsp_apply_swap(cand, n, i, j);
                flen = mlab_tsp_length(inst, cand);
                ++fevals;
                if (cfg->max_fevals > 0 && fevals > cfg->max_fevals + n * n)
                    break;
                /* 特赦：改善全局最优则允许禁忌移动 */
                aspir = (flen < f_best - 1e-12);
                if (is_tabu && !aspir) continue;
                /* 长期记忆：频次惩罚只影响选择，不影响 f_best 口径 */
                eff = flen + lambda * (double)freq[idx];
                if (eff < best_eff - 1e-12) {
                    best_eff = eff;
                    best_flen = flen;
                    bi = i;
                    bj = j;
                    best_tabu = is_tabu && aspir;
                    n_ties = 1;
                } else if (eff <= best_eff + 1e-12) {
                    /* 等效移动随机 tie-break：两次种子走出不同轨迹 */
                    ++n_ties;
                    if (mlab_rng_uniform(rng) < 1.0 / (double)n_ties) {
                        best_eff = eff;
                        best_flen = flen;
                        bi = i;
                        bj = j;
                        best_tabu = is_tabu && aspir;
                    }
                }
            }
        }

        if (bi < 0) {
            /* 全禁忌 fallback：随机逃逸，仍记入禁忌表与 f_best 口径 */
            int a = (int)(mlab_rng_uniform(rng) * n);
            int b = (int)(mlab_rng_uniform(rng) * n);
            int idx;
            if (a == b) b = (b + 1) % n;
            if (b >= n) b = 0;
            idx = pair_index(a, b, n);
            memcpy(cand, cur, (size_t)n * sizeof(int));
            if (cfg->use_2opt)
                mlab_tsp_apply_2opt(cand, n, a, b);
            else
                mlab_tsp_apply_swap(cand, n, a, b);
            memcpy(cur, cand, (size_t)n * sizeof(int));
            f_cur = mlab_tsp_length(inst, cur);
            ++fevals;
            ++out->n_fallback;
            if (tenure > 0)
                tabu_until[idx] = it + tenure;
            freq[idx] += 1;
            if (f_cur < f_best) {
                f_best = f_cur;
                memcpy(best, cur, (size_t)n * sizeof(int));
            }
        } else {
            int idx = pair_index(bi, bj, n);
            memcpy(cand, cur, (size_t)n * sizeof(int));
            if (cfg->use_2opt)
                mlab_tsp_apply_2opt(cand, n, bi, bj);
            else
                mlab_tsp_apply_swap(cand, n, bi, bj);
            memcpy(cur, cand, (size_t)n * sizeof(int));
            f_cur = best_flen; /* 已评估 */
            if (best_tabu) ++out->n_tabu_overrides;
            if (freq[bi * n + bj] > 0) ++out->n_freq_picks;
            if (tenure > 0)
                tabu_until[idx] = it + tenure;
            freq[idx] += 1;
            if (f_cur < f_best) {
                f_best = f_cur;
                memcpy(best, cur, (size_t)n * sizeof(int));
            }
        }
        out->n_iter = it;
    }
    out->best_len = f_best;
    out->n_fevals = (int)fevals;
    free(cur);
    free(cand);
    free(tabu_until);
    free(freq);
    return 1;
}
