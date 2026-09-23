#include "sgd.h"

static int apply_update(int kind, double *w, sgd_state *st, const double *g,
                        int dim, double lr, double wd);

#include <math.h>
#include <stdlib.h>
#include <string.h>

int sgd_state_init(sgd_state *st, int dim)
{
    st->m = (double *)calloc((size_t)dim, sizeof(double));
    st->v = (double *)calloc((size_t)dim, sizeof(double));
    st->t = 0.0;
    return (st->m && st->v) ? 0 : -1;
}

void sgd_state_free(sgd_state *st)
{
    free(st->m);
    free(st->v);
    st->m = st->v = NULL;
}

int sgd_step(const sgd_problem *p, double *w, sgd_state *st,
                  const double *X, const double *Y, int m, int dim,
                  int batch, double lr, int kind, double wd)
{
    double *g = (double *)malloc((size_t)dim * sizeof(double));
    int i, b, rc;
    if (!g) return -1;
    memset(g, 0, (size_t)dim * sizeof(double));
    if (batch <= 0 || batch > m) batch = m;
    for (b = 0; b < batch; ++b) {
        double *gb = (double *)malloc((size_t)dim * sizeof(double));
        if (!gb) {
            free(g);
            return -1;
        }
        /* use sample b (caller shuffles or this is cyclic) */
        p->grad(w, X + (size_t)b * dim, Y + b, dim, gb, p->ctx);
        for (i = 0; i < dim; ++i) g[i] += gb[i];
        free(gb);
    }
    for (i = 0; i < dim; ++i) g[i] /= batch;
    rc = apply_update(kind, w, st, g, dim, lr, wd);
    free(g);
    return rc;
}


/* ---- 更新规则抽取（sgd_step 与 sgd_epoch 共用） ---- */

static int apply_update(int kind, double *w, sgd_state *st, const double *g,
                        int dim, double lr, double wd)
{
    int i;
    const double beta1 = 0.9, beta2 = 0.999, eps = 1e-8;
    st->t += 1.0;
    switch (kind) {
    case 0: /* SGD */
        for (i = 0; i < dim; ++i) w[i] -= lr * g[i];
        break;
    case 1: /* Polyak momentum */
        for (i = 0; i < dim; ++i) {
            st->m[i] = beta1 * st->m[i] + g[i];
            w[i] -= lr * st->m[i];
        }
        break;
    case 2: /* Nesterov */
        for (i = 0; i < dim; ++i) {
            double m_prev = st->m[i];
            st->m[i] = beta1 * st->m[i] + g[i];
            w[i] -= lr * (-beta1 * m_prev + (1 + beta1) * st->m[i]);
        }
        break;
    case 3: /* AdaGrad */
        for (i = 0; i < dim; ++i) {
            st->v[i] += g[i] * g[i];
            w[i] -= lr * g[i] / (sqrt(st->v[i]) + eps);
        }
        break;
    case 4: /* RMSProp */
        for (i = 0; i < dim; ++i) {
            st->v[i] = beta2 * st->v[i] + (1 - beta2) * g[i] * g[i];
            w[i] -= lr * g[i] / (sqrt(st->v[i]) + eps);
        }
        break;
    case 5: /* Adam */
        for (i = 0; i < dim; ++i) {
            st->m[i] = beta1 * st->m[i] + (1 - beta1) * g[i];
            st->v[i] = beta2 * st->v[i] + (1 - beta2) * g[i] * g[i];
            {
                double mh = st->m[i] / (1 - pow(beta1, st->t));
                double vh = st->v[i] / (1 - pow(beta2, st->t));
                w[i] -= lr * mh / (sqrt(vh) + eps);
            }
        }
        break;
    case 6: /* AdamW */
        for (i = 0; i < dim; ++i) {
            st->m[i] = beta1 * st->m[i] + (1 - beta1) * g[i];
            st->v[i] = beta2 * st->v[i] + (1 - beta2) * g[i] * g[i];
            {
                double mh = st->m[i] / (1 - pow(beta1, st->t));
                double vh = st->v[i] / (1 - pow(beta2, st->t));
                w[i] -= lr * mh / (sqrt(vh) + eps) + lr * wd * w[i];
            }
        }
        break;
    default:
        return -1;
    }
    return 0;
}


double sgd_sched_lr(const sgd_lr_sched *s, long k)
{
    if (!s || s->kind == 0) return s ? s->lr0 : 0.0;
    if (k < 1) k = 1;
    if (s->kind == 1) return s->lr0 / (double)k;         /* ∝ 1/k */
    return s->lr0 / sqrt((double)k);                      /* ∝ 1/sqrt(k) */
}

double sgd_full_loss(const sgd_problem *p, const double *w, const double *X,
                     const double *Y, int m, int dim)
{
    double s = 0.0;
    int i;
    for (i = 0; i < m; ++i)
        s += p->loss(w, X + (size_t)i * dim, Y + i, dim, p->ctx);
    return s / m;
}

int sgd_epoch(const sgd_problem *p, double *w, sgd_state *st,
              const double *X, const double *Y, int m, int dim,
              int batch, const sgd_lr_sched *sched, int kind, double wd,
              mlab_rng *rng, long *step_counter, double *loss_out)
{
    int *idx = (int *)malloc((size_t)m * sizeof(int));
    double *g = (double *)malloc((size_t)dim * sizeof(double));
    int i, b, start;
    if (!idx || !g) {
        free(idx); free(g);
        return -1;
    }
    if (batch <= 0 || batch > m) batch = m;
    /* Fisher-Yates 洗牌（固定 seed 的 rng → 可复现） */
    for (i = 0; i < m; ++i) idx[i] = i;
    for (i = m - 1; i > 0; --i) {
        int j = (int)(mlab_rng_uniform(rng) * (double)(i + 1));
        int t;
        if (j > i) j = i;
        t = idx[i]; idx[i] = idx[j]; idx[j] = t;
    }
    for (start = 0; start < m; start += batch) {
        int cnt = (start + batch <= m) ? batch : (m - start);
        double lr;
        memset(g, 0, (size_t)dim * sizeof(double));
        for (b = 0; b < cnt; ++b) {
            int id = idx[start + b];
            double *gb = (double *)malloc((size_t)dim * sizeof(double));
            if (!gb) {
                free(idx); free(g);
                return -1;
            }
            p->grad(w, X + (size_t)id * dim, Y + id, dim, gb, p->ctx);
            for (i = 0; i < dim; ++i) g[i] += gb[i];
            free(gb);
        }
        for (i = 0; i < dim; ++i) g[i] /= cnt;
        lr = sgd_sched_lr(sched, *step_counter);
        ++(*step_counter);
        if (apply_update(kind, w, st, g, dim, lr, wd) != 0) {
            free(idx); free(g);
            return -1;
        }
    }
    if (loss_out) *loss_out = sgd_full_loss(p, w, X, Y, m, dim);
    free(idx); free(g);
    return 0;
}

int mlab_svrg(const sgd_problem *p, double *w, const double *X, const double *Y,
              int m, int dim, int epochs, double lr, mlab_rng *rng,
              double *loss_hist, int hist_cap, int *hist_len)
{
    double *w_tilde = (double *)malloc((size_t)dim * sizeof(double));
    double *mu = (double *)malloc((size_t)dim * sizeof(double));
    double *g = (double *)malloc((size_t)dim * sizeof(double));
    double *gt = (double *)malloc((size_t)dim * sizeof(double));
    int *idx = (int *)malloc((size_t)m * sizeof(int));
    int e, i, k, hl = 0;
    if (!w_tilde || !mu || !g || !gt || !idx) {
        free(w_tilde); free(mu); free(g); free(gt); free(idx);
        return -1;
    }
    memcpy(w_tilde, w, (size_t)dim * sizeof(double));
    for (e = 0; e < epochs; ++e) {
        /* 全梯度 μ = ∇F(w̃) */
        memset(mu, 0, (size_t)dim * sizeof(double));
        for (i = 0; i < m; ++i) {
            p->grad(w_tilde, X + (size_t)i * dim, Y + i, dim, gt, p->ctx);
            for (k = 0; k < dim; ++k) mu[k] += gt[k];
        }
        for (k = 0; k < dim; ++k) mu[k] /= m;
        /* 洗牌 */
        for (i = 0; i < m; ++i) idx[i] = i;
        for (i = m - 1; i > 0; --i) {
            int j = (int)(mlab_rng_uniform(rng) * (double)(i + 1));
            int t;
            if (j > i) j = i;
            t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
        for (i = 0; i < m; ++i) {
            int id = idx[i];
            p->grad(w, X + (size_t)id * dim, Y + id, dim, g, p->ctx);
            p->grad(w_tilde, X + (size_t)id * dim, Y + id, dim, gt, p->ctx);
            for (k = 0; k < dim; ++k) g[k] = g[k] - gt[k] + mu[k];
            for (k = 0; k < dim; ++k) w[k] -= lr * g[k];
        }
        memcpy(w_tilde, w, (size_t)dim * sizeof(double));
        if (loss_hist && hl < hist_cap)
            loss_hist[hl++] = sgd_full_loss(p, w, X, Y, m, dim);
    }
    if (hist_len) *hist_len = hl;
    free(w_tilde); free(mu); free(g); free(gt); free(idx);
    return 0;
}
