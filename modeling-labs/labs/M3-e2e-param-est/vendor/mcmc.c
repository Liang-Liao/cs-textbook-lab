#include "mcmc.h"
#include "stats.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef MLAB_PI
#define MLAB_PI 3.14159265358979323846
#endif

void mlab_chain_free(mlab_chain *c)
{
    if (!c) return;
    free(c->x);
    c->x = NULL;
    c->n_keep = 0;
    c->n_prop = c->n_accept = 0;
}

static int chain_alloc(mlab_chain *out, int d, int n_keep, int n_burn, int thin)
{
    if (!out || d <= 0 || n_keep <= 0 || thin <= 0) return 0;
    memset(out, 0, sizeof *out);
    out->x = (double *)malloc((size_t)n_keep * (size_t)d * sizeof(double));
    if (!out->x) return 0;
    out->d = d;
    out->n_keep = n_keep;
    out->n_burn = n_burn;
    out->thin = thin;
    return 1;
}

void mlab_chain_mean(const mlab_chain *c, double *mean_out)
{
    int i, j;
    if (!c || !c->x || !mean_out) return;
    for (j = 0; j < c->d; ++j) mean_out[j] = 0.0;
    for (i = 0; i < c->n_keep; ++i)
        for (j = 0; j < c->d; ++j)
            mean_out[j] += c->x[(size_t)i * c->d + j];
    if (c->n_keep > 0)
        for (j = 0; j < c->d; ++j) mean_out[j] /= c->n_keep;
}

void mlab_chain_cov(const mlab_chain *c, double *cov_out)
{
    double *mu;
    int i, j, k;
    if (!c || !c->x || !cov_out || c->n_keep < 2) return;
    mu = (double *)malloc((size_t)c->d * sizeof(double));
    if (!mu) return;
    mlab_chain_mean(c, mu);
    for (j = 0; j < c->d * c->d; ++j) cov_out[j] = 0.0;
    for (i = 0; i < c->n_keep; ++i) {
        for (j = 0; j < c->d; ++j) {
            double dj = c->x[(size_t)i * c->d + j] - mu[j];
            for (k = 0; k < c->d; ++k) {
                double dk = c->x[(size_t)i * c->d + k] - mu[k];
                cov_out[j * c->d + k] += dj * dk;
            }
        }
    }
    for (j = 0; j < c->d * c->d; ++j)
        cov_out[j] /= (c->n_keep - 1);
    free(mu);
}

double mlab_chain_acf(const mlab_chain *c, int dim, int lag)
{
    double mean, num = 0.0, den = 0.0;
    int i, n;
    if (!c || !c->x || dim < 0 || dim >= c->d) return 0.0;
    n = c->n_keep;
    if (n < 2 || lag < 0 || lag >= n) return lag == 0 ? 1.0 : 0.0;
    mean = 0.0;
    for (i = 0; i < n; ++i) mean += c->x[(size_t)i * c->d + dim];
    mean /= n;
    for (i = 0; i < n; ++i) {
        double d0 = c->x[(size_t)i * c->d + dim] - mean;
        den += d0 * d0;
        if (i + lag < n)
            num += d0 * (c->x[(size_t)(i + lag) * c->d + dim] - mean);
    }
    if (den <= 0.0) return 0.0;
    return num / den;
}

double mlab_chain_iat(const mlab_chain *c, int dim, int max_lag)
{
    int n, k, kmax;
    double tau, prev_pair;
    if (!c || !c->x || c->n_keep < 4) return 1.0;
    n = c->n_keep;
    kmax = max_lag > 0 ? max_lag : (n / 2 > 500 ? 500 : n / 2);
    if (kmax < 2) kmax = 2;
    if (kmax > n - 2) kmax = n - 2;
    tau = 1.0;
    prev_pair = 1e300;
    /* Geyer 初始正序列：Γ_k = ρ_{2k-1} + ρ_{2k} */
    for (k = 1; 2 * k <= kmax; ++k) {
        double r1 = mlab_chain_acf(c, dim, 2 * k - 1);
        double r2 = mlab_chain_acf(c, dim, 2 * k);
        double pair = r1 + r2;
        if (pair <= 0.0) break;
        if (pair > prev_pair) pair = prev_pair;
        tau += 2.0 * pair;
        prev_pair = pair;
    }
    return tau > 1.0 ? tau : 1.0;
}

double mlab_gelman_rubin(const double *const *chains, int m, int n, int d,
                         double *rhat_per_dim)
{
    int j, i, k;
    double rmax = 0.0;
    if (!chains || m < 2 || n < 2 || d <= 0) return 0.0;
    for (k = 0; k < d; ++k) {
        double W = 0.0, B = 0.0, mean_all = 0.0, V, R;
        double *cj_mean = (double *)malloc((size_t)m * sizeof(double));
        double *cj_var = (double *)malloc((size_t)m * sizeof(double));
        if (!cj_mean || !cj_var) {
            free(cj_mean);
            free(cj_var);
            return 0.0;
        }
        for (j = 0; j < m; ++j) {
            const double *x = chains[j];
            double s = 0.0, s2 = 0.0;
            for (i = 0; i < n; ++i) {
                double v = x[(size_t)i * d + k];
                s += v;
                s2 += v * v;
            }
            cj_mean[j] = s / n;
            cj_var[j] = (s2 - s * s / n) / (n - 1);
            mean_all += cj_mean[j];
        }
        mean_all /= m;
        for (j = 0; j < m; ++j) {
            double dm = cj_mean[j] - mean_all;
            W += cj_var[j];
            B += dm * dm;
        }
        W /= m;
        B = B * n / (m - 1);
        if (W <= 0.0) {
            R = 1.0;
        } else {
            V = ((double)(n - 1) / n) * W + B / n;
            R = sqrt(V / W);
        }
        if (rhat_per_dim) rhat_per_dim[k] = R;
        if (R > rmax) rmax = R;
        free(cj_mean);
        free(cj_var);
    }
    return rmax;
}

static void store_state(mlab_chain *out, int slot, const double *x)
{
    memcpy(out->x + (size_t)slot * out->d, x, (size_t)out->d * sizeof(double));
}

static void finish_chain(mlab_chain *out, long n_prop, long n_acc)
{
    out->n_prop = n_prop;
    out->n_accept = n_acc;
    out->accept_rate = n_prop > 0 ? (double)n_acc / (double)n_prop : 0.0;
}

int mlab_mh_rwm(mlab_rng *rng,
                mlab_log_density_fn log_pi, void *ctx,
                const double *x0, int d,
                double step,
                int n_burn, int n_keep, int thin,
                mlab_chain *out)
{
    double *x, *y;
    double lp_x, lp_y;
    long t, n_prop, n_acc = 0;
    int j, stored = 0;

    if (!rng || !log_pi || !x0 || d <= 0 || step <= 0 || !out) return 0;
    if (!chain_alloc(out, d, n_keep, n_burn, thin)) return 0;
    x = (double *)malloc((size_t)d * sizeof(double));
    y = (double *)malloc((size_t)d * sizeof(double));
    if (!x || !y) {
        free(x);
        free(y);
        mlab_chain_free(out);
        return 0;
    }
    memcpy(x, x0, (size_t)d * sizeof(double));
    lp_x = log_pi(x, d, ctx);
    n_prop = (long)n_burn + (long)n_keep * thin;
    for (t = 0; t < n_prop; ++t) {
        double u, log_alpha;
        for (j = 0; j < d; ++j)
            y[j] = x[j] + step * mlab_rng_normal(rng);
        lp_y = log_pi(y, d, ctx);
        log_alpha = lp_y - lp_x;
        u = mlab_rng_uniform(rng);
        if (log(u) < log_alpha) {
            memcpy(x, y, (size_t)d * sizeof(double));
            lp_x = lp_y;
            ++n_acc;
        }
        if (t >= n_burn && ((t - n_burn) % thin == 0) && stored < n_keep) {
            store_state(out, stored++, x);
        }
    }
    finish_chain(out, n_prop, n_acc);
    free(x);
    free(y);
    return 1;
}

int mlab_mh_independent(mlab_rng *rng,
                        mlab_log_density_fn log_pi, void *ctx,
                        const double *prop_mu, double prop_sigma,
                        const double *x0, int d,
                        int n_burn, int n_keep, int thin,
                        mlab_chain *out)
{
    double *x, *y;
    double lp_x, lp_y, c_q;
    long t, n_prop, n_acc = 0;
    int j, stored = 0;

    if (!rng || !log_pi || !x0 || !prop_mu || d <= 0 || prop_sigma <= 0 || !out)
        return 0;
    if (!chain_alloc(out, d, n_keep, n_burn, thin)) return 0;
    x = (double *)malloc((size_t)d * sizeof(double));
    y = (double *)malloc((size_t)d * sizeof(double));
    if (!x || !y) {
        free(x);
        free(y);
        mlab_chain_free(out);
        return 0;
    }
    memcpy(x, x0, (size_t)d * sizeof(double));
    lp_x = log_pi(x, d, ctx);
    /* log N(x; mu, σ²I) 中与 x 有关的项：-0.5/σ² ||x-mu||² */
    c_q = 0.5 / (prop_sigma * prop_sigma);
    n_prop = (long)n_burn + (long)n_keep * thin;
    for (t = 0; t < n_prop; ++t) {
        double log_qx = 0.0, log_qy = 0.0, log_alpha, u;
        for (j = 0; j < d; ++j) {
            double dx = x[j] - prop_mu[j];
            double dy;
            y[j] = prop_mu[j] + prop_sigma * mlab_rng_normal(rng);
            dy = y[j] - prop_mu[j];
            log_qx += dx * dx;
            log_qy += dy * dy;
        }
        log_qx *= -c_q;
        log_qy *= -c_q;
        lp_y = log_pi(y, d, ctx);
        log_alpha = (lp_y + log_qx) - (lp_x + log_qy);
        u = mlab_rng_uniform(rng);
        if (log(u) < log_alpha) {
            memcpy(x, y, (size_t)d * sizeof(double));
            lp_x = lp_y;
            ++n_acc;
        }
        if (t >= n_burn && ((t - n_burn) % thin == 0) && stored < n_keep) {
            store_state(out, stored++, x);
        }
    }
    finish_chain(out, n_prop, n_acc);
    free(x);
    free(y);
    return 1;
}

int mlab_hmc(mlab_rng *rng,
             mlab_log_density_fn log_pi, mlab_grad_fn grad_log_pi, void *ctx,
             const double *x0, int d,
             double step, int n_leapfrog,
             int n_burn, int n_keep, int thin,
             mlab_chain *out)
{
    double *x, *y, *p, *g;
    double lp_x;
    long t, n_prop, n_acc = 0;
    int j, stored = 0;

    if (!rng || !log_pi || !grad_log_pi || !x0 || d <= 0 || step <= 0 ||
        n_leapfrog < 1 || !out)
        return 0;
    if (!chain_alloc(out, d, n_keep, n_burn, thin)) return 0;
    x = (double *)malloc((size_t)d * sizeof(double));
    y = (double *)malloc((size_t)d * sizeof(double));
    p = (double *)malloc((size_t)d * sizeof(double));
    g = (double *)malloc((size_t)d * sizeof(double));
    if (!x || !y || !p || !g) {
        free(x); free(y); free(p); free(g);
        mlab_chain_free(out);
        return 0;
    }
    memcpy(x, x0, (size_t)d * sizeof(double));
    lp_x = log_pi(x, d, ctx);
    grad_log_pi(x, d, ctx, g);
    n_prop = (long)n_burn + (long)n_keep * thin;
    for (t = 0; t < n_prop; ++t) {
        double h0, ke, log_alpha, u;
        for (j = 0; j < d; ++j) p[j] = mlab_rng_normal(rng);
        h0 = -lp_x;
        for (j = 0; j < d; ++j) h0 += 0.5 * p[j] * p[j];
        memcpy(y, x, (size_t)d * sizeof(double));
        /* leapfrog：半步动量 → 整步位置（含梯度） → … → 半步动量 */
        for (j = 0; j < d; ++j) p[j] += 0.5 * step * g[j];
        for (j = 0; j < d; ++j) y[j] += step * p[j];
        {
            int l;
            for (l = 1; l < n_leapfrog; ++l) {
                grad_log_pi(y, d, ctx, g);
                for (j = 0; j < d; ++j) p[j] += step * g[j];
                for (j = 0; j < d; ++j) y[j] += step * p[j];
            }
        }
        grad_log_pi(y, d, ctx, g);
        for (j = 0; j < d; ++j) p[j] += 0.5 * step * g[j];
        {
            double lp_y = log_pi(y, d, ctx);
            ke = 0.0;
            for (j = 0; j < d; ++j) ke += p[j] * p[j];
            log_alpha = h0 - (-lp_y + 0.5 * ke);
            u = mlab_rng_uniform(rng);
            if (log(u) < log_alpha) {
                memcpy(x, y, (size_t)d * sizeof(double));
                lp_x = lp_y;
                ++n_acc;
            } else {
                grad_log_pi(x, d, ctx, g); /* 恢复当前点的梯度 */
            }
        }
        if (t >= n_burn && ((t - n_burn) % thin == 0) && stored < n_keep) {
            store_state(out, stored++, x);
        }
    }
    finish_chain(out, n_prop, n_acc);
    free(x);
    free(y);
    free(p);
    free(g);
    return 1;
}

int mlab_gibbs_bvn(mlab_rng *rng,
                   const double mu[2], double s1, double s2, double rho,
                   const double x0[2],
                   int n_burn, int n_keep, int thin,
                   mlab_chain *out)
{
    double x[2];
    double v1 = s1 * s1 * (1.0 - rho * rho);
    double v2 = s2 * s2 * (1.0 - rho * rho);
    double sd1 = sqrt(v1 > 0 ? v1 : 0.0);
    double sd2 = sqrt(v2 > 0 ? v2 : 0.0);
    double b12 = rho * s1 / s2; /* x1|x2 回归系数 */
    double b21 = rho * s2 / s1;
    long t, n_prop;
    int stored = 0;

    if (!rng || !mu || !x0 || !out) return 0;
    if (s1 <= 0 || s2 <= 0 || fabs(rho) >= 1.0) return 0;
    if (!chain_alloc(out, 2, n_keep, n_burn, thin)) return 0;
    x[0] = x0[0];
    x[1] = x0[1];
    n_prop = (long)n_burn + (long)n_keep * thin;
    for (t = 0; t < n_prop; ++t) {
        double m1 = mu[0] + b12 * (x[1] - mu[1]);
        double m2;
        /* 顺序 Gibbs：x1|x2(旧) → x2|x1(新) */
        x[0] = m1 + sd1 * mlab_rng_normal(rng);
        m2 = mu[1] + b21 * (x[0] - mu[0]);
        x[1] = m2 + sd2 * mlab_rng_normal(rng);
        if (t >= n_burn && ((t - n_burn) % thin == 0) && stored < n_keep) {
            store_state(out, stored, x);
            ++stored;
        }
    }
    out->n_prop = n_prop;
    out->n_accept = n_prop; /* Gibbs 总是“接受”一次条件更新 */
    out->accept_rate = 1.0;
    return 1;
}

double mlab_bvn_logpdf(const double *x, int d, void *ctx)
{
    const mlab_bvn_ctx *b = (const mlab_bvn_ctx *)ctx;
    double z1, z2, q, det;
    if (!x || d < 2 || !b) return -1e300;
    z1 = (x[0] - b->mu[0]) / b->s1;
    z2 = (x[1] - b->mu[1]) / b->s2;
    det = 1.0 - b->rho * b->rho;
    if (det <= 0.0) return -1e300;
    q = (z1 * z1 - 2.0 * b->rho * z1 * z2 + z2 * z2) / det;
    return -0.5 * q;
}

void mlab_bvn_conjugate_posterior(const double mu0[2], double s0,
                                  const double s1, const double s2,
                                  const double rho,
                                  const double *data, int n, int d,
                                  mlab_bvn_posterior *out)
{
    /*
     * 先验 μ ~ N(mu0, s0² I)。
     * 似然 x_i | μ ~ N(μ, Σ)，Σ = [[s1², ρ s1 s2],[ρ s1 s2, s2²]]。
     * 精度：Λ0 = I/s0²，Λ = Σ^{-1}，Λn = Λ0 + n Λ。
     * μn = Λn^{-1} (Λ0 mu0 + n Λ x̄)。
     * 协方差 Σn = Λn^{-1}，再参数化为 (s1n, s2n, rho_n)。
     */
    double L0, L11, L12, L22, Ln11, Ln12, Ln22;
    double det_n, inv11, inv12, inv22;
    double xbar[2] = {0, 0};
    double rhs0, rhs1, det;
    int i;

    if (!mu0 || !out || n < 1 || !data || d < 2 || s0 <= 0 || s1 <= 0 || s2 <= 0)
        return;
    det = 1.0 - rho * rho;
    if (det <= 0.0) return;
    for (i = 0; i < n; ++i) {
        xbar[0] += data[(size_t)i * d + 0];
        xbar[1] += data[(size_t)i * d + 1];
    }
    xbar[0] /= n;
    xbar[1] /= n;

    L0 = 1.0 / (s0 * s0);
    /* Σ^{-1} = 1/det * [[1/s1², -ρ/(s1 s2)],[-ρ/(s1 s2), 1/s2²]] */
    L11 = (1.0 / (s1 * s1)) / det;
    L22 = (1.0 / (s2 * s2)) / det;
    L12 = (-rho / (s1 * s2)) / det;

    Ln11 = L0 + n * L11;
    Ln22 = L0 + n * L22;
    Ln12 = n * L12;
    det_n = Ln11 * Ln22 - Ln12 * Ln12;
    if (det_n == 0.0) return;
    inv11 = Ln22 / det_n;
    inv22 = Ln11 / det_n;
    inv12 = -Ln12 / det_n;

    rhs0 = L0 * mu0[0] + n * (L11 * xbar[0] + L12 * xbar[1]);
    rhs1 = L0 * mu0[1] + n * (L12 * xbar[0] + L22 * xbar[1]);

    out->mu[0] = inv11 * rhs0 + inv12 * rhs1;
    out->mu[1] = inv12 * rhs0 + inv22 * rhs1;
    out->s1 = sqrt(inv11 > 0 ? inv11 : 0.0);
    out->s2 = sqrt(inv22 > 0 ? inv22 : 0.0);
    if (out->s1 > 0 && out->s2 > 0)
        out->rho = inv12 / (out->s1 * out->s2);
    else
        out->rho = 0.0;
}
