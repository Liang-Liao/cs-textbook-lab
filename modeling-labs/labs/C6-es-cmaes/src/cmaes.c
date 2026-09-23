#include "cmaes.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef MLAB_PI
#define MLAB_PI 3.14159265358979323846
#endif

void mlab_cmaes_result_free(mlab_cmaes_result *r)
{
    if (!r) return;
    free(r->best_x);
    free(r->best_hist);
    free(r->sigma_hist);
    free(r->angle_hist);
    free(r->principal);
    r->best_x = NULL;
    r->best_hist = NULL;
    r->sigma_hist = NULL;
    r->angle_hist = NULL;
    r->principal = NULL;
}

double mlab_axis_angle_deg(const double *v, const double *target, int n)
{
    double nv = 0.0, nt = 0.0, dot = 0.0, c;
    int i;
    if (!v || !target || n <= 0) return 90.0;
    for (i = 0; i < n; ++i) {
        nv += v[i] * v[i];
        nt += target[i] * target[i];
        dot += v[i] * target[i];
    }
    if (nv <= 0.0 || nt <= 0.0) return 90.0;
    c = fabs(dot) / (sqrt(nv) * sqrt(nt));
    if (c > 1.0) c = 1.0;
    return acos(c) * 180.0 / MLAB_PI;
}

int mlab_symeig_jacobi(double *A, int n, double *evals, double *V)
{
    int sweep, p, q, i, j, max_sweeps = 80;
    if (!A || !evals || !V || n <= 0) return 0;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j)
            V[i * n + j] = (i == j) ? 1.0 : 0.0;
    }
    for (sweep = 0; sweep < max_sweeps; ++sweep) {
        double off = 0.0;
        for (p = 0; p < n; ++p)
            for (q = p + 1; q < n; ++q)
                off += A[p * n + q] * A[p * n + q];
        if (off < 1e-28) break;
        for (p = 0; p < n - 1; ++p) {
            for (q = p + 1; q < n; ++q) {
                double apq = A[p * n + q];
                double app, aqq, tau, t, c, s;
                if (fabs(apq) < 1e-300) continue;
                app = A[p * n + p];
                aqq = A[q * n + q];
                tau = (aqq - app) / (2.0 * apq);
                t = (tau >= 0.0 ? 1.0 : -1.0) / (fabs(tau) + sqrt(1.0 + tau * tau));
                c = 1.0 / sqrt(1.0 + t * t);
                s = t * c;
                for (i = 0; i < n; ++i) {
                    double aip = A[i * n + p];
                    double aiq = A[i * n + q];
                    A[i * n + p] = c * aip - s * aiq;
                    A[i * n + q] = s * aip + c * aiq;
                }
                for (i = 0; i < n; ++i) {
                    double api = A[p * n + i];
                    double aqi = A[q * n + i];
                    A[p * n + i] = c * api - s * aqi;
                    A[q * n + i] = s * api + c * aqi;
                }
                for (i = 0; i < n; ++i) {
                    double vip = V[i * n + p];
                    double viq = V[i * n + q];
                    V[i * n + p] = c * vip - s * viq;
                    V[i * n + q] = s * vip + c * viq;
                }
            }
        }
    }
    /* 特征值升序排序（同步排列 V 列） */
    for (i = 0; i < n; ++i) evals[i] = A[i * n + i];
    for (i = 0; i < n; ++i) {
        int k = i;
        for (j = i + 1; j < n; ++j)
            if (evals[j] < evals[k]) k = j;
        if (k != i) {
            double tmp = evals[i];
            evals[i] = evals[k];
            evals[k] = tmp;
            for (j = 0; j < n; ++j) {
                tmp = V[j * n + i];
                V[j * n + i] = V[j * n + k];
                V[j * n + k] = tmp;
            }
        }
    }
    return 1;
}

static void reflect_box(double *x, int dim, const double *lb, const double *ub)
{
    int d;
    if (!lb || !ub) return;
    for (d = 0; d < dim; ++d) {
        double lo = lb[d], hi = ub[d];
        if (hi <= lo) { x[d] = lo; continue; }
        /* 多次反射到区间内 */
        while (x[d] < lo || x[d] > hi) {
            if (x[d] < lo) x[d] = 2.0 * lo - x[d];
            if (x[d] > hi) x[d] = 2.0 * hi - x[d];
        }
    }
}

static double dot_n(const double *a, const double *b, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

int mlab_cmaes_run(mlab_rng *rng, const mlab_cmaes_config *cfg,
                   const double *x0, mlab_cmaes_result *out)
{
    int dim, lam, mu, g, i, k, d, n_eval = 0;
    double *m, *m_old, *C, *B, *D, *pc, *ps, *w;
    double *arx, *ary, *arz, *arfit, *idx;
    double *y_w, *tmp, *Csqrt_inv_y;
    double sigma, cs, ds, cc, c1, cmu, mu_eff, chiN;
    double stop_f;
    int max_hist, hist_cap;

    if (!rng || !cfg || !cfg->f || !out || cfg->dim <= 0 || cfg->max_evals <= 0)
        return 0;
    dim = cfg->dim;
    stop_f = cfg->stop_f;
    memset(out, 0, sizeof *out);
    out->evals_to_target = -1;
    out->reached = 0;

    lam = 4 + (int)floor(3.0 * log((double)dim));
    if (lam < 4) lam = 4;
    if (cfg->lambda_mul > 1) lam *= cfg->lambda_mul; /* IPOP 重启倍率 */
    mu = lam / 2;

    /* Hansen 默认学习率 */
    {
        double N = (double)dim;
        mu_eff = 0.0;
        w = (double *)malloc((size_t)mu * sizeof(double));
        if (!w) return 0;
        for (i = 0; i < mu; ++i) {
            w[i] = log((double)mu + 0.5) - log((double)(i + 1));
            mu_eff += w[i];
        }
        /* 先归一化权重，再算 mu_eff = 1/sum w^2 */
        {
            double sw = 0.0, sw2 = 0.0;
            for (i = 0; i < mu; ++i) sw += w[i];
            for (i = 0; i < mu; ++i) w[i] /= sw;
            for (i = 0; i < mu; ++i) sw2 += w[i] * w[i];
            mu_eff = 1.0 / sw2;
        }
        cc = (4.0 + mu_eff / N) / (N + 4.0 + 2.0 * mu_eff / N);
        c1 = 2.0 / ((N + 1.3) * (N + 1.3) + mu_eff);
        cmu = fmin(1.0 - c1,
                   2.0 * (mu_eff - 2.0 + 1.0 / mu_eff) / ((N + 2.0) * (N + 2.0) + mu_eff));
        if (cmu < 0.0) cmu = 0.0;
        cs = (mu_eff + 2.0) / (N + mu_eff + 5.0);
        ds = 1.0 + 2.0 * fmax(0.0, sqrt((mu_eff - 1.0) / (N + 1.0)) - 1.0) + cs;
        chiN = sqrt(N) * (1.0 - 1.0 / (4.0 * N) + 1.0 / (21.0 * N * N));
    }

    hist_cap = cfg->max_evals / lam + 4;
    if (hist_cap < 8) hist_cap = 8;
    max_hist = hist_cap;

    m = (double *)malloc((size_t)dim * sizeof(double));
    m_old = (double *)malloc((size_t)dim * sizeof(double));
    C = (double *)calloc((size_t)dim * dim, sizeof(double));
    B = (double *)calloc((size_t)dim * dim, sizeof(double));
    D = (double *)malloc((size_t)dim * sizeof(double));
    pc = (double *)calloc((size_t)dim, sizeof(double));
    ps = (double *)calloc((size_t)dim, sizeof(double));
    arx = (double *)malloc((size_t)lam * dim * sizeof(double));
    ary = (double *)malloc((size_t)lam * dim * sizeof(double));
    arz = (double *)malloc((size_t)lam * dim * sizeof(double));
    arfit = (double *)malloc((size_t)lam * sizeof(double));
    idx = (double *)malloc((size_t)lam * sizeof(double));
    y_w = (double *)malloc((size_t)dim * sizeof(double));
    tmp = (double *)malloc((size_t)dim * sizeof(double));
    Csqrt_inv_y = (double *)malloc((size_t)dim * sizeof(double));
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->best_hist = (double *)malloc((size_t)max_hist * sizeof(double));
    out->sigma_hist = (double *)malloc((size_t)max_hist * sizeof(double));
    out->principal = (double *)malloc((size_t)dim * sizeof(double));
    if (cfg->track_angle && dim == 2)
        out->angle_hist = (double *)malloc((size_t)max_hist * sizeof(double));

    if (!m || !m_old || !C || !B || !D || !pc || !ps || !arx || !ary || !arz ||
        !arfit || !idx || !y_w || !tmp || !Csqrt_inv_y || !out->best_x ||
        !out->best_hist || !out->sigma_hist || !out->principal) {
        free(w);
        mlab_cmaes_result_free(out);
        return 0;
    }

    /* m0 */
    if (x0) {
        memcpy(m, x0, (size_t)dim * sizeof(double));
    } else {
        double s = cfg->x0_scale > 0.0 ? cfg->x0_scale : 2.0;
        for (d = 0; d < dim; ++d)
            m[d] = (2.0 * mlab_rng_uniform(rng) - 1.0) * s;
    }
    if (cfg->lb && cfg->ub) reflect_box(m, dim, cfg->lb, cfg->ub);
    sigma = cfg->sigma0 > 0.0 ? cfg->sigma0 : 0.5;
    for (i = 0; i < dim; ++i) {
        C[i * dim + i] = 1.0;
        B[i * dim + i] = 1.0;
        D[i] = 1.0;
    }

    out->best_f = 1e300;
    g = 0;
    while (n_eval < cfg->max_evals) {
        int n_gen_eval = 0;
        double nps, hsig, norm_ps;

        /* 采样 λ 个个体（受预算截断） */
        for (k = 0; k < lam && n_eval < cfg->max_evals; ++k) {
            double fs;
            for (d = 0; d < dim; ++d)
                arz[k * dim + d] = mlab_rng_normal(rng);
            /* y = B (D .* z) */
            for (d = 0; d < dim; ++d) tmp[d] = D[d] * arz[k * dim + d];
            for (d = 0; d < dim; ++d) {
                double s = 0.0;
                for (i = 0; i < dim; ++i) s += B[d * dim + i] * tmp[i];
                /* B stored row-major; eigenvectors are columns → B[d,i] = V[d*dim+i]
                   y_d = sum_i V[d,i] * (D_i z_i) = sum_i B[d,i] * tmp[i] */
                ary[k * dim + d] = s;
            }
            for (d = 0; d < dim; ++d)
                arx[k * dim + d] = m[d] + sigma * ary[k * dim + d];
            if (cfg->lb && cfg->ub)
                reflect_box(&arx[k * dim], dim, cfg->lb, cfg->ub);
            /* 反射后用实际 x 重算 y 会偏；仍用未反射 y 更新（常见近似）。
               为对齐：若反射发生，用 (x-m)/sigma 作为 y */
            if (cfg->lb && cfg->ub) {
                for (d = 0; d < dim; ++d)
                    ary[k * dim + d] = (arx[k * dim + d] - m[d]) / sigma;
            }
            fs = cfg->f(&arx[k * dim], dim, cfg->ctx);
            ++n_eval;
            ++n_gen_eval;
            arfit[k] = fs;
            idx[k] = (double)k;
            if (fs < out->best_f) {
                out->best_f = fs;
                memcpy(out->best_x, &arx[k * dim], (size_t)dim * sizeof(double));
                if (stop_f > 0.0 && fs < stop_f && !out->reached) {
                    out->reached = 1;
                    out->evals_to_target = n_eval;
                }
            }
        }
        if (n_gen_eval <= 0) break;

        /* 按适应度排序（插入排序，个体数很小） */
        for (i = 1; i < n_gen_eval; ++i) {
            double fi = arfit[i], ii = idx[i];
            int j = i - 1;
            while (j >= 0 && arfit[j] > fi) {
                arfit[j + 1] = arfit[j];
                idx[j + 1] = idx[j];
                --j;
            }
            arfit[j + 1] = fi;
            idx[j + 1] = ii;
        }

        memcpy(m_old, m, (size_t)dim * sizeof(double));
        memset(m, 0, (size_t)dim * sizeof(double));
        {
            int mu_use = mu < n_gen_eval ? mu : n_gen_eval;
            double sw = 0.0;
            for (i = 0; i < mu_use; ++i) sw += w[i];
            if (sw <= 0.0) sw = 1.0;
            for (i = 0; i < mu_use; ++i) {
                int ii = (int)idx[i];
                for (d = 0; d < dim; ++d)
                    m[d] += (w[i] / sw) * arx[ii * dim + d];
            }
        }
        for (d = 0; d < dim; ++d)
            y_w[d] = (m[d] - m_old[d]) / sigma;

        /* C^{-1/2} y_w = B * (D^{-1} .* (B^T y_w)) */
        for (d = 0; d < dim; ++d) {
            double s = 0.0;
            for (i = 0; i < dim; ++i) s += B[i * dim + d] * y_w[i];
            tmp[d] = s / (D[d] > 1e-12 ? D[d] : 1e-12);
        }
        for (d = 0; d < dim; ++d) {
            double s = 0.0;
            for (i = 0; i < dim; ++i) s += B[d * dim + i] * tmp[i];
            Csqrt_inv_y[d] = s;
        }

        /* 进化路径 ps、σ */
        for (d = 0; d < dim; ++d)
            ps[d] = (1.0 - cs) * ps[d] + sqrt(cs * (2.0 - cs) * mu_eff) * Csqrt_inv_y[d];
        norm_ps = sqrt(dot_n(ps, ps, dim));
        sigma *= exp((cs / ds) * (norm_ps / chiN - 1.0));
        if (sigma < 1e-12) sigma = 1e-12;
        if (sigma > 1e30) sigma = 1e30;

        nps = norm_ps / sqrt(1.0 - pow(1.0 - cs, 2.0 * (double)(g + 1)));
        hsig = (nps < (1.4 + 2.0 / ((double)dim + 1.0)) * chiN) ? 1.0 : 0.0;

        for (d = 0; d < dim; ++d)
            pc[d] = (1.0 - cc) * pc[d] +
                    hsig * sqrt(cc * (2.0 - cc) * mu_eff) * y_w[d];

        /* 秩-μ + 秩-1 更新 C */
        {
            double decay = 1.0 - c1 - cmu + c1 * (1.0 - hsig) * cc * (2.0 - cc);
            if (decay < 0.0) decay = 0.0;
            for (i = 0; i < dim * dim; ++i) C[i] *= decay;
            for (d = 0; d < dim; ++d)
                for (i = 0; i < dim; ++i)
                    C[d * dim + i] += c1 * pc[d] * pc[i];
            {
                int mu_use = mu < n_gen_eval ? mu : n_gen_eval;
                double sw = 0.0;
                for (k = 0; k < mu_use; ++k) sw += w[k];
                if (sw <= 0.0) sw = 1.0;
                for (k = 0; k < mu_use; ++k) {
                    int ii = (int)idx[k];
                    double wk = w[k] / sw;
                    for (d = 0; d < dim; ++d)
                        for (i = 0; i < dim; ++i)
                            C[d * dim + i] += cmu * wk *
                                ary[ii * dim + d] * ary[ii * dim + i];
                }
            }
            /* 对称化 + 数值下限 */
            for (d = 0; d < dim; ++d) {
                for (i = d + 1; i < dim; ++i) {
                    double avg = 0.5 * (C[d * dim + i] + C[i * dim + d]);
                    C[d * dim + i] = avg;
                    C[i * dim + d] = avg;
                }
                if (C[d * dim + d] < 1e-300) C[d * dim + d] = 1e-300;
            }
        }

        /* 特征分解 → B, D */
        {
            double *Cw = (double *)malloc((size_t)dim * dim * sizeof(double));
            double *ev = (double *)malloc((size_t)dim * sizeof(double));
            double *V = (double *)malloc((size_t)dim * dim * sizeof(double));
            if (!Cw || !ev || !V) {
                free(Cw); free(ev); free(V);
                break;
            }
            memcpy(Cw, C, (size_t)dim * dim * sizeof(double));
            if (!mlab_symeig_jacobi(Cw, dim, ev, V)) {
                free(Cw); free(ev); free(V);
                break;
            }
            memcpy(B, V, (size_t)dim * dim * sizeof(double));
            for (d = 0; d < dim; ++d) {
                double e = ev[d] > 1e-300 ? ev[d] : 1e-300;
                D[d] = sqrt(e);
            }
            /* evals 升序；最大特征向量 = 最后一列：component d = V[d*dim + dim-1] */
            for (d = 0; d < dim; ++d)
                out->principal[d] = V[d * dim + (dim - 1)];
            out->cond_C = D[dim - 1] / (D[0] > 1e-12 ? D[0] : 1e-12);
            free(Cw); free(ev); free(V);
        }

        if (out->hist_len < max_hist) {
            out->best_hist[out->hist_len] = out->best_f;
            out->sigma_hist[out->hist_len] = sigma;
            if (out->angle_hist && dim == 2) {
                double ang = mlab_axis_angle_deg(out->principal, cfg->major_axis, 2);
                out->angle_hist[out->hist_len] = ang;
            }
            ++out->hist_len;
        }
        out->gen_used = g + 1;
        ++g;

        if (stop_f > 0.0 && out->reached) break;
    }

    out->n_eval = n_eval;
    free(w);
    free(m); free(m_old); free(C); free(B); free(D);
    free(pc); free(ps); free(arx); free(ary); free(arz);
    free(arfit); free(idx); free(y_w); free(tmp); free(Csqrt_inv_y);
    return 1;
}

/* ---------- IPOP 重启（增种群预算分配） ---------- */

int mlab_cmaes_run_ipop(mlab_rng *rng, const mlab_cmaes_config *cfg,
                        mlab_cmaes_result *out)
{
    mlab_cmaes_config sub;
    int budget_left, total_evals = 0, mul, dim, rc = 1;

    if (!rng || !cfg || !out || cfg->dim <= 0 || cfg->max_evals <= 0)
        return 0;
    dim = cfg->dim;
    memset(out, 0, sizeof *out);
    out->evals_to_target = -1;
    out->best_f = 1e300;
    out->best_x = (double *)malloc((size_t)dim * sizeof(double));
    out->principal = (double *)malloc((size_t)dim * sizeof(double));
    if (!out->best_x || !out->principal) {
        mlab_cmaes_result_free(out);
        return 0;
    }

    /* IPOP：内部无停滞停止准则，按段分配总预算（默认 4 段均分），
     * λ 逐段倍增；某段达标即提前结束，剩余预算不再消耗 */
    {
        const int n_seg = 4;
        int seg = 0;
        budget_left = cfg->max_evals;
        mul = 1;
        while (seg < n_seg && budget_left > 0 && mul <= 64) {
            mlab_cmaes_result r;
            int seg_budget = budget_left / (n_seg - seg);
            memset(&r, 0, sizeof r);
            sub = *cfg;
            sub.max_evals = seg_budget;
            sub.lambda_mul = mul;
            if (!mlab_cmaes_run(rng, &sub, NULL, &r)) {
                rc = 0;
                break;
            }
            budget_left -= r.n_eval;
            total_evals += r.n_eval;
            if (r.best_f < out->best_f) {
                out->best_f = r.best_f;
                memcpy(out->best_x, r.best_x, (size_t)dim * sizeof(double));
                memcpy(out->principal, r.principal, (size_t)dim * sizeof(double));
                out->cond_C = r.cond_C;
            }
            if (!out->reached && r.reached) {
                out->reached = 1;
                out->evals_to_target = total_evals - r.n_eval + r.evals_to_target;
            }
            mlab_cmaes_result_free(&r);
            if (out->reached) break;
            mul *= 2;
            ++seg;
        }
    }
    out->n_eval = total_evals;
    return rc;
}
