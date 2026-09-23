#include "pso.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_pso_result_free(mlab_pso_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->best_hist);
    r->best_x = NULL;
    r->best_hist = NULL;
}

const char *mlab_pso_topo_name(mlab_pso_topo t)
{
    if (t == MLAB_PSO_TOPO_RING) return "ring";
    if (t == MLAB_PSO_TOPO_VON_NEUMANN) return "von_neumann";
    return "global";
}

const char *mlab_pso_mode_name(mlab_pso_mode m)
{
    if (m == MLAB_PSO_MODE_COG_ONLY) return "cog_only";
    if (m == MLAB_PSO_MODE_SOC_ONLY) return "soc_only";
    return "full";
}

int mlab_pso_run(mlab_rng *rng, const mlab_pso_config *cfg,
                 const double *x0_swarm, mlab_pso_result *out)
{
    int dim, np, g, i, d, max_gen, cols, rows;
    double *x, *v, *pbest, *pfit, *nbest_x;
    double w0, w1, c1, c2, wmax_scale;
    int hist_cap, constriction;
    double chi;

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0 ||
        cfg->swarm < 2 || !cfg->lb || !cfg->ub)
        return 0;
    dim = cfg->dim;
    np = cfg->swarm;
    max_gen = cfg->max_gen > 0 ? cfg->max_gen : 100;
    constriction = cfg->use_constriction ? 1 : 0;
    chi = cfg->chi > 0.0 ? cfg->chi : 0.729;
    w0 = cfg->w > 0.0 ? cfg->w : 0.72;
    w1 = cfg->w_linear ? (cfg->w_end > 0.0 ? cfg->w_end : 0.4) : w0;
    c1 = cfg->c1;
    c2 = cfg->c2;
    if (cfg->mode == MLAB_PSO_MODE_COG_ONLY) c2 = 0.0;
    if (cfg->mode == MLAB_PSO_MODE_SOC_ONLY) c1 = 0.0;
    if (c1 < 0.0) c1 = 0.0;
    if (c2 < 0.0) c2 = 0.0;
    /* 默认经典 c1=c2=2.0（若未设置） */
    if (cfg->c1 == 0.0 && cfg->c2 == 0.0 && cfg->mode == MLAB_PSO_MODE_FULL) {
        c1 = 2.0;
        c2 = 2.0;
    }
    wmax_scale = cfg->vmax_scale > 0.0 ? cfg->vmax_scale : 0.2;
    /* Clerc 收缩因子：FULL 模式下切换到经典无钳制参数化
     * χ=0.729, c1=c2=1.4962（等价 w=1），收敛性有理论保证、无需 vmax */
    if (constriction && cfg->mode == MLAB_PSO_MODE_FULL) {
        w0 = 1.0;
        w1 = 1.0;
        c1 = 1.4962;
        c2 = 1.4962;
    }
    cols = (int)sqrt((double)np);
    if (cols < 1) cols = 1;
    rows = (np + cols - 1) / cols;

    memset(out, 0, sizeof *out);
    out->reached = 0;
    hist_cap = max_gen + 2;
    x = (double *)malloc((size_t)np * dim * sizeof(double));
    v = (double *)malloc((size_t)np * dim * sizeof(double));
    pbest = (double *)malloc((size_t)np * dim * sizeof(double));
    pfit = (double *)malloc((size_t)np * sizeof(double));
    nbest_x = (double *)malloc((size_t)dim * sizeof(double));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->best_hist = (double *)malloc((size_t)hist_cap * sizeof(double));
    if (!x || !v || !pbest || !pfit || !nbest_x || !out->best_x || !out->best_hist) {
        free(x); free(v); free(pbest); free(pfit); free(nbest_x);
        mlab_pso_result_free(out);
        return 0;
    }

    for (i = 0; i < np; ++i) {
        for (d = 0; d < dim; ++d) {
            double lo = cfg->lb[d], hi = cfg->ub[d];
            double span = hi - lo;
            if (x0_swarm) {
                x[i * dim + d] = x0_swarm[i * dim + d];
            } else {
                x[i * dim + d] = lo + span * mlab_rng_uniform(rng);
            }
            v[i * dim + d] = (2.0 * mlab_rng_uniform(rng) - 1.0) * wmax_scale * span;
            pbest[i * dim + d] = x[i * dim + d];
        }
        pfit[i] = cfg->f(&x[i * dim], dim, cfg->ctx);
        ++out->n_eval;
    }

    out->best_f = 1e300;
    for (i = 0; i < np; ++i) {
        if (pfit[i] < out->best_f) {
            out->best_f = pfit[i];
            memcpy(out->best_x, &pbest[i * dim], (size_t)dim * sizeof(double));
        }
    }

    for (g = 0; g < max_gen; ++g) {
        double w = (max_gen <= 1) ? w0
            : w0 + (w1 - w0) * ((double)g / (double)(max_gen - 1));
        /* 邻域最优（环型：自身+左右；星型：全局） */
        if (out->hist_len < hist_cap) {
            out->best_hist[out->hist_len++] = out->best_f;
        }
        if (cfg->stop_f > 0.0 && out->best_f < cfg->stop_f) {
            out->reached = 1;
            break;
        }

        for (i = 0; i < np; ++i) {
            int nidx = i;
            if (cfg->topo == MLAB_PSO_TOPO_RING) {
                int left = (i - 1 + np) % np;
                int right = (i + 1) % np;
                nidx = i;
                if (pfit[left] < pfit[nidx]) nidx = left;
                if (pfit[right] < pfit[nidx]) nidx = right;
            } else if (cfg->topo == MLAB_PSO_TOPO_VON_NEUMANN) {
                /* 粒子按 rows×cols 网格排布（末行不满时索引对 np 取模） */
                int r = i / cols, c = i % cols;
                int left = ((r * cols) + (c + cols - 1) % cols) % np;
                int right = ((r * cols) + (c + 1) % cols) % np;
                int up = (((r + rows - 1) % rows) * cols + c) % np;
                int down = (((r + 1) % rows) * cols + c) % np;
                nidx = i;
                if (pfit[left] < pfit[nidx]) nidx = left;
                if (pfit[right] < pfit[nidx]) nidx = right;
                if (pfit[up] < pfit[nidx]) nidx = up;
                if (pfit[down] < pfit[nidx]) nidx = down;
            } else {
                /* 全局：用当前 best 位置 */
                nidx = -1;
            }
            if (nidx >= 0)
                memcpy(nbest_x, &pbest[nidx * dim], (size_t)dim * sizeof(double));
            else
                memcpy(nbest_x, out->best_x, (size_t)dim * sizeof(double));

            for (d = 0; d < dim; ++d) {
                double lo = cfg->lb[d], hi = cfg->ub[d];
                double span = hi - lo;
                double vmax = wmax_scale * span;
                double r1 = mlab_rng_uniform(rng);
                double r2 = mlab_rng_uniform(rng);
                double vi = w * v[i * dim + d]
                    + c1 * r1 * (pbest[i * dim + d] - x[i * dim + d])
                    + c2 * r2 * (nbest_x[d] - x[i * dim + d]);
                if (constriction) {
                    vi *= chi;
                } else {
                    if (vi > vmax) vi = vmax;
                    if (vi < -vmax) vi = -vmax;
                }
                v[i * dim + d] = vi;
                x[i * dim + d] += vi;
                /* 边界：吸收 + 钳制 */
                if (x[i * dim + d] < lo) {
                    x[i * dim + d] = lo;
                    v[i * dim + d] = -0.5 * v[i * dim + d];
                } else if (x[i * dim + d] > hi) {
                    x[i * dim + d] = hi;
                    v[i * dim + d] = -0.5 * v[i * dim + d];
                }
            }
            {
                double fi = cfg->f(&x[i * dim], dim, cfg->ctx);
                ++out->n_eval;
                if (fi < pfit[i]) {
                    pfit[i] = fi;
                    memcpy(&pbest[i * dim], &x[i * dim], (size_t)dim * sizeof(double));
                    if (fi < out->best_f) {
                        out->best_f = fi;
                        memcpy(out->best_x, &pbest[i * dim], (size_t)dim * sizeof(double));
                    }
                }
            }
        }
        out->gen_used = g + 1;
    }
    if (cfg->stop_f > 0.0 && out->best_f < cfg->stop_f)
        out->reached = 1;

    free(x); free(v); free(pbest); free(pfit); free(nbest_x);
    return 1;
}
