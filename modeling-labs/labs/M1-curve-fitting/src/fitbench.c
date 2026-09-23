#include "fitbench.h"
#include "linalg.h"
#include "rng.h"
#include "optcore.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double mlab_fit_model_eval(int model_id, const double *t, const double *theta,
                           int npar, void *ctx)
{
    (void)ctx;
    (void)npar;
    if (model_id == MLAB_FIT_MODEL_EXP) {
        return theta[0] * exp(-theta[1] * t[0]);
    }
    /* exp-sin: a exp(-b t) sin(c t + d) */
    return theta[0] * exp(-theta[1] * t[0]) *
           sin(theta[2] * t[0] + theta[3]);
}

int mlab_fit_synth(mlab_fit_data *d, unsigned seed)
{
    int i;
    mlab_rng rng;
    if (!d || !d->t || d->m <= 0 || d->npar <= 0 || !d->theta_true) return -1;
    if (!d->y) {
        d->y = (double *)malloc((size_t)d->m * sizeof(double));
        if (!d->y) return -1;
    }
    if (!d->y_clean) {
        d->y_clean = (double *)malloc((size_t)d->m * sizeof(double));
        if (!d->y_clean) return -1;
    }
    mlab_rng_seed(&rng, seed);
    for (i = 0; i < d->m; ++i) {
        d->y_clean[i] = mlab_fit_model_eval(d->model_id, d->t + i,
                                            d->theta_true, d->npar, NULL);
        d->y[i] = d->y_clean[i] + d->sigma * mlab_rng_normal(&rng);
    }
    return 0;
}

int mlab_fit_result_alloc(mlab_fit_result *r, int npar)
{
    if (!r || npar <= 0) return -1;
    memset(r, 0, sizeof *r);
    r->npar = npar;
    r->theta = (double *)calloc((size_t)npar, sizeof(double));
    r->cov = (double *)calloc((size_t)npar * (size_t)npar, sizeof(double));
    r->se = (double *)calloc((size_t)npar, sizeof(double));
    if (!r->theta || !r->cov || !r->se) {
        mlab_fit_result_free(r);
        return -1;
    }
    return 0;
}

void mlab_fit_result_free(mlab_fit_result *r)
{
    if (!r) return;
    free(r->theta);
    free(r->cov);
    free(r->se);
    r->theta = r->cov = r->se = NULL;
}

int mlab_fit_covariance(const double *t, const double *y, int m,
                        int model_id, const double *theta, int npar,
                        double sigma2, double *cov_out, double *se_out)
{
    double *J = NULL, *JtJ = NULL, *inv = NULL;
    int i, j, k, rc = 0;
    (void)y;

    J = (double *)malloc((size_t)m * (size_t)npar * sizeof(double));
    JtJ = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
    inv = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
    if (!J || !JtJ || !inv) {
        free(J); free(JtJ); free(inv);
        return -1;
    }
    /* FD 雅可比 */
    for (j = 0; j < npar; ++j) {
        double *thp = (double *)malloc((size_t)npar * sizeof(double));
        double eps, save;
        if (!thp) {
            free(J); free(JtJ); free(inv);
            return -1;
        }
        memcpy(thp, theta, (size_t)npar * sizeof(double));
        eps = 1e-6 * (fabs(theta[j]) + 1.0);
        save = thp[j];
        for (i = 0; i < m; ++i) {
            double f0, f1;
            thp[j] = save;
            f0 = mlab_fit_model_eval(model_id, t + i, thp, npar, NULL);
            thp[j] = save + eps;
            f1 = mlab_fit_model_eval(model_id, t + i, thp, npar, NULL);
            J[i * npar + j] = (f1 - f0) / eps;
        }
        free(thp);
    }
    for (j = 0; j < npar; ++j) {
        for (k = 0; k < npar; ++k) {
            double s = 0.0;
            for (i = 0; i < m; ++i) s += J[i * npar + j] * J[i * npar + k];
            JtJ[j * npar + k] = s;
        }
    }
    /* inv(JtJ) */
    for (j = 0; j < npar * npar; ++j) inv[j] = JtJ[j];
    {
        double *I = (double *)malloc((size_t)npar * (size_t)npar * sizeof(double));
        if (!I) {
            free(J); free(JtJ); free(inv);
            return -1;
        }
        memset(I, 0, (size_t)npar * (size_t)npar * sizeof(double));
        for (j = 0; j < npar; ++j) I[j * npar + j] = 1.0;
        /* Gauss-Jordan on inv | I using inv as work A */
        {
            double *A = (double *)malloc((size_t)npar * (size_t)2 * npar * sizeof(double));
            if (!A) {
                free(I); free(J); free(JtJ); free(inv);
                return -1;
            }
            for (j = 0; j < npar; ++j) {
                for (k = 0; k < npar; ++k) A[j * 2 * npar + k] = inv[j * npar + k];
                for (k = 0; k < npar; ++k) A[j * 2 * npar + npar + k] = I[j * npar + k];
            }
            for (k = 0; k < npar; ++k) {
                int piv = k;
                double amax = fabs(A[k * 2 * npar + k]);
                for (i = k + 1; i < npar; ++i) {
                    double v = fabs(A[i * 2 * npar + k]);
                    if (v > amax) { amax = v; piv = i; }
                }
                if (amax < 1e-300) {
                    free(A); free(I); free(J); free(JtJ); free(inv);
                    return -1;
                }
                if (piv != k) {
                    for (j = 0; j < 2 * npar; ++j) {
                        double tmp = A[k * 2 * npar + j];
                        A[k * 2 * npar + j] = A[piv * 2 * npar + j];
                        A[piv * 2 * npar + j] = tmp;
                    }
                }
                {
                    double d = A[k * 2 * npar + k];
                    for (j = 0; j < 2 * npar; ++j) A[k * 2 * npar + j] /= d;
                }
                for (i = 0; i < npar; ++i) {
                    double f;
                    if (i == k) continue;
                    f = A[i * 2 * npar + k];
                    for (j = 0; j < 2 * npar; ++j)
                        A[i * 2 * npar + j] -= f * A[k * 2 * npar + j];
                }
            }
            for (j = 0; j < npar; ++j)
                for (k = 0; k < npar; ++k)
                    inv[j * npar + k] = A[j * 2 * npar + npar + k];
            free(A);
        }
        free(I);
    }
    if (cov_out) {
        for (j = 0; j < npar; ++j)
            for (k = 0; k < npar; ++k)
                cov_out[j * npar + k] = sigma2 * inv[j * npar + k];
    }
    if (se_out) {
        for (j = 0; j < npar; ++j) {
            double v = sigma2 * inv[j * npar + j];
            se_out[j] = v > 0 ? sqrt(v) : 0.0;
        }
    }
    free(J); free(JtJ); free(inv);
    return rc;
}

static void fill_rss_stats(const double *t, const double *y, int m,
                           int model_id, mlab_fit_result *r)
{
    int i;
    double rss = 0.0;
    for (i = 0; i < m; ++i) {
        double e = mlab_fit_model_eval(model_id, t + i, r->theta, r->npar, NULL) - y[i];
        rss += e * e;
    }
    r->rss = rss;
    r->rmse = sqrt(rss / (double)m);
    r->sigma2 = (m > r->npar) ? rss / (double)(m - r->npar) : rss / (double)m;
}

/* BFGS 目标上下文（fevals 由 optcore run 计数，不在 ctx 重复累计） */
typedef struct {
    const double *t;
    const double *y;
    int m;
    int model_id;
    int npar;
} fit_obj_ctx;

static double fit_obj_f(const double *x, int n, void *ctx)
{
    fit_obj_ctx *c = (fit_obj_ctx *)ctx;
    double s = 0.0;
    int i;
    (void)n;
    for (i = 0; i < c->m; ++i) {
        double e = mlab_fit_model_eval(c->model_id, c->t + i, x, c->npar, NULL) - c->y[i];
        s += e * e;
    }
    return 0.5 * s;
}

static void fit_obj_grad(const double *x, int n, double *g, void *ctx)
{
    fit_obj_ctx *c = (fit_obj_ctx *)ctx;
    int j, i;
    (void)n;
    /* 解析梯度（EXP）：y=a e^{-bt}, ∂/∂a=e^{-bt}, ∂/∂b=-a t e^{-bt} */
    if (c->model_id == MLAB_FIT_MODEL_EXP && c->npar >= 2) {
        double a = x[0], b = x[1];
        g[0] = 0.0;
        g[1] = 0.0;
        for (i = 0; i < c->m; ++i) {
            double ebt = exp(-b * c->t[i]);
            double yh = a * ebt;
            double r = yh - c->y[i];
            g[0] += r * ebt;
            g[1] += r * (-a * c->t[i] * ebt);
        }
        for (j = 2; j < c->npar; ++j) g[j] = 0.0;
        return;
    }
    /* 其他模型：中心差分 */
    for (j = 0; j < c->npar; ++j) {
        double *xp = (double *)malloc((size_t)c->npar * sizeof(double));
        double *xm = (double *)malloc((size_t)c->npar * sizeof(double));
        double eps, save;
        if (!xp || !xm) {
            free(xp); free(xm);
            g[j] = 0.0;
            continue;
        }
        memcpy(xp, x, (size_t)c->npar * sizeof(double));
        memcpy(xm, x, (size_t)c->npar * sizeof(double));
        eps = 1e-6 * (fabs(x[j]) + 1.0);
        save = x[j];
        xp[j] = save + eps;
        xm[j] = save - eps;
        g[j] = 0.0;
        for (i = 0; i < c->m; ++i) {
            double mid = mlab_fit_model_eval(c->model_id, c->t + i, x, c->npar, NULL);
            double mp = mlab_fit_model_eval(c->model_id, c->t + i, xp, c->npar, NULL);
            double mm = mlab_fit_model_eval(c->model_id, c->t + i, xm, c->npar, NULL);
            double r = mid - c->y[i];
            double dm = (mp - mm) / (2.0 * eps);
            g[j] += r * dm;
        }
        free(xp);
        free(xm);
    }
}

int mlab_fit_bfgs(const double *t, const double *y, int m,
                  int model_id, double *theta, int npar,
                  int max_iter, double tol, mlab_fit_result *out)
{
    fit_obj_ctx ctx;
    mlab_objective obj;
    mlab_opt_run run;
    int rc;

    if (!t || !y || !theta || !out) return -1;
    ctx.t = t;
    ctx.y = y;
    ctx.m = m;
    ctx.model_id = model_id;
    ctx.npar = npar;
    memset(&obj, 0, sizeof obj);
    obj.dim = npar;
    obj.f = fit_obj_f;
    obj.grad = fit_obj_grad;
    obj.hess = NULL;
    obj.ctx = &ctx;
    mlab_opt_run_init(&run, NULL, 0);
    memcpy(out->theta, theta, (size_t)npar * sizeof(double));
    rc = mlab_opt_bfgs(&obj, out->theta, tol, max_iter, &run);
    memcpy(theta, out->theta, (size_t)npar * sizeof(double));
    fill_rss_stats(t, y, m, model_id, out);
    out->engine = "bfgs";
    out->iters = run.iters;
    /* optcore run.fevals 已计入每次目标求值（含线搜索），不再叠加 ctx 计数避免双计 */
    out->fevals = run.fevals;
    out->status = (rc == 0 && run.status == 0) ? 0 : 1;
    mlab_fit_covariance(t, y, m, model_id, out->theta, npar,
                        out->sigma2, out->cov, out->se);
    return (out->status == 0) ? 0 : -1;
}

/* 适配 mlab_nl_model：ctx 指向 int model_id */
static double lm_model_adapter(const double *t, const double *theta, int npar, void *ctx)
{
    int model_id = ctx ? *(int *)ctx : MLAB_FIT_MODEL_EXP;
    return mlab_fit_model_eval(model_id, t, theta, npar, NULL);
}

int mlab_fit_lm(const double *t, const double *y, int m,
                int model_id, double *theta, int npar,
                int max_iter, double tol, mlab_fit_result *out)
{
    int it = 0, rc;
    int mid = model_id;
    if (!t || !y || !theta || !out) return -1;
    memcpy(out->theta, theta, (size_t)npar * sizeof(double));
    rc = mlab_lm(t, y, m, lm_model_adapter, &mid,
                 out->theta, npar, max_iter, tol, &it);
    memcpy(theta, out->theta, (size_t)npar * sizeof(double));
    fill_rss_stats(t, y, m, model_id, out);
    out->engine = "lm";
    out->iters = it;
    out->fevals = 0;
    out->status = (rc == 0) ? 0 : 1;
    mlab_fit_covariance(t, y, m, model_id, out->theta, npar,
                        out->sigma2, out->cov, out->se);
    return (out->status == 0) ? 0 : -1;
}

/* ---------- Student-t 97.5% 分位（不完全 Beta 求逆，仅依赖 libm） ---------- */

/* Lentz 加速连分式：betacf(a,b,x)（NR 风格） */
static double fitbetacf(double a, double b, double x)
{
    const int MAXIT = 300;
    const double EPS = 3e-14;
    const double FPMIN = 1e-300;
    int m, m2;
    double aa, c, d, del, h, qab, qam, qap;

    qab = a + b;
    qap = a + 1.0;
    qam = a - 1.0;
    c = 1.0;
    d = 1.0 - qab * x / qap;
    if (fabs(d) < FPMIN) d = FPMIN;
    d = 1.0 / d;
    h = d;
    for (m = 1; m <= MAXIT; ++m) {
        m2 = 2 * m;
        aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < FPMIN) d = FPMIN;
        c = 1.0 + aa / c;
        if (fabs(c) < FPMIN) c = FPMIN;
        d = 1.0 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1.0 + aa * d;
        if (fabs(d) < FPMIN) d = FPMIN;
        c = 1.0 + aa / c;
        if (fabs(c) < FPMIN) c = FPMIN;
        d = 1.0 / d;
        del = d * c;
        h *= del;
        if (fabs(del - 1.0) < EPS) break;
    }
    if (m > MAXIT) return NAN;
    return h;
}

/* 正则化不完全 Beta 函数 I_x(a,b) */
static double fitibeta(double a, double b, double x)
{
    double bt;
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    bt = exp(lgamma(a + b) - lgamma(a) - lgamma(b)
             + a * log(x) + b * log1p(-x));
    if (x < (a + 1.0) / (a + b + 2.0))
        return bt * fitbetacf(a, b, x) / a;
    return 1.0 - bt * fitbetacf(b, a, 1.0 - x) / b;
}

double mlab_fit_t_crit95(int dof)
{
    const double q = 0.05;  /* P(T>t)=0.025 → I_x(dof/2,1/2)=2*0.025 */
    double a, lo, hi, x;
    int i;
    if (dof <= 0 || dof >= 100000) return 1.959964; /* 退化为正态 */
    a = 0.5 * (double)dof;
    /* 二分求 x：I_x(a, 1/2) = q，再 t = sqrt(dof (1-x)/x) */
    lo = 0.0;
    hi = 1.0;
    for (i = 0; i < 100; ++i) {
        x = 0.5 * (lo + hi);
        if (fitibeta(a, 0.5, x) < q) lo = x;
        else hi = x;
    }
    x = 0.5 * (lo + hi);
    return sqrt((double)dof * (1.0 - x) / x);
}

int mlab_fit_ci95_contains(const mlab_fit_result *r, const double *theta_true,
                            int dof, int *hit_out)
{
    int j, all = 1;
    double tc;
    if (!r || !theta_true || !r->se) return -1;
    tc = mlab_fit_t_crit95(dof);
    for (j = 0; j < r->npar; ++j) {
        double lo = r->theta[j] - tc * r->se[j];
        double hi = r->theta[j] + tc * r->se[j];
        int hit = (theta_true[j] >= lo) && (theta_true[j] <= hi);
        if (hit_out) hit_out[j] = hit;
        if (!hit) all = 0;
    }
    return all;
}

int mlab_fit_write_report(const char *path, const char *title,
                          const mlab_fit_data *d, const mlab_fit_result *r,
                          int append)
{
    FILE *fp;
    int i, j;
    if (!path || !d || !r) return -1;
    fp = fopen(path, append ? "a" : "w");
    if (!fp) return -1;
    fprintf(fp, "# %s\n\n", title ? title : "fit report");
    fprintf(fp, "engine=%s status=%d iters=%d m=%d npar=%d sigma=%.4g\n",
            r->engine ? r->engine : "?", r->status, r->iters, d->m, r->npar, d->sigma);
    fprintf(fp, "RSS=%.8g RMSE=%.8g sigma2=%.8g\n\n", r->rss, r->rmse, r->sigma2);
    fprintf(fp, "## parameters\n");
    fprintf(fp, "j,theta,se,true,err\n");
    for (j = 0; j < r->npar; ++j)
        fprintf(fp, "%d,%.8g,%.8g,%.8g,%.8g\n",
                j, r->theta[j], r->se[j], d->theta_true[j],
                r->theta[j] - d->theta_true[j]);
    fprintf(fp, "\n## covariance\n");
    for (i = 0; i < r->npar; ++i) {
        for (j = 0; j < r->npar; ++j)
            fprintf(fp, "%s%.8g", j ? "," : "", r->cov[i * r->npar + j]);
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n## residuals (first min(m,20))\n");
    fprintf(fp, "t,y,yhat,resid\n");
    for (i = 0; i < d->m && i < 20; ++i) {
        double yh = mlab_fit_model_eval(d->model_id, d->t + i, r->theta, r->npar, NULL);
        fprintf(fp, "%.6f,%.6f,%.6f,%.6f\n", d->t[i], d->y[i], yh, d->y[i] - yh);
    }
    fprintf(fp, "\n");
    fclose(fp);
    return 0;
}
