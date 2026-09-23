#include "optcore.h"
#include "bench.h"
#include "conv.h"
#include "linalg.h"
#include "linesearch.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void run_reset(mlab_opt_run *run)
{
    double *fh;
    int hl;
    if (!run) return;
    /* Preserve history buffer set by mlab_opt_run_init; callers must init first. */
    fh = run->f_hist;
    hl = run->hist_len;
    memset(run, 0, sizeof *run);
    run->f_hist = fh;
    run->hist_len = hl;
}

void mlab_opt_run_init(mlab_opt_run *run, double *f_hist, int hist_len)
{
    memset(run, 0, sizeof *run);
    run->f_hist = f_hist;
    run->hist_len = hist_len;
}

static void hist_push(mlab_opt_run *run, int k, double f)
{
    if (run && run->f_hist && run->hist_len > 0 && k >= 0 && k < run->hist_len)
        run->f_hist[k] = f;
}

int mlab_opt_gd(const mlab_objective *obj, double *x, double tol,
                int max_iter, mlab_opt_run *run)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    int it;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g) {
        run->status = -1;
        return -1;
    }
    run->status = 1;
    for (it = 0; it < max_iter; ++it) {
        int i;
        double gn, alpha;
        obj->grad(x, n, g, obj->ctx);
        ++run->gevals;
        run->f_final = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, run->f_final);
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        for (i = 0; i < n; ++i) g[i] = -g[i];
        {
            int fe_alpha = 0;
            alpha = mlab_armijo_backtrack(obj, x, g, 1.0, 1e-4, 40, &fe_alpha);
            run->fevals += fe_alpha;
        }
        if (alpha <= 0) {
            /* 线搜索失败（含非下降方向/无可接受步长）：如实报未收敛 */
            run->iters = it + 1;
            run->status = 1;
            break;
        }
        mlab_vec_axpy(x, alpha, g, n);
        run->iters = it + 1;
    }
    free(g);
    return run->status == 0 ? 0 : 1;
}

/* Modify H to be PD by adding ridge */
static int make_pd(double *H, int n)
{
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double ridge = 0.0;
    int k, ok = 0;
    if (!L) return -1;
    for (k = 0; k < 30; ++k) {
        if (mlab_cholesky(H, n, L) == 0) {
            ok = 1;
            break;
        }
        ridge = (ridge == 0.0) ? 1e-8 : ridge * 10.0;
        {
            int i;
            for (i = 0; i < n; ++i) H[i * n + i] += ridge;
        }
    }
    free(L);
    return ok ? 0 : -1;
}

int mlab_opt_newton(const mlab_objective *obj, double *x, double tol,
                    int max_iter, mlab_opt_run *run)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *H = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *p = (double *)malloc((size_t)n * sizeof(double));
    int it;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g || !H || !L || !p) {
        free(g); free(H); free(L); free(p);
        run->status = -1;
        return -1;
    }
    run->status = 1;
    for (it = 0; it < max_iter; ++it) {
        double gn, alpha;
        int i;
        obj->grad(x, n, g, obj->ctx);
        ++run->gevals;
        run->f_final = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, run->f_final);
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        if (obj->hess) obj->hess(x, n, H, obj->ctx);
        else {
            /* 有限差分 Hessian fallback：对梯度做中心差分（列 j） */
            const double h0 = 1e-5;
            int a, b;
            for (b = 0; b < n; ++b) {
                double xp = x[b], h = h0 * (1.0 + fabs(x[b]));
                double *xpj = (double *)malloc((size_t)n * sizeof(double));
                double *gp, *gm;
                if (!xpj) break;
                memcpy(xpj, x, (size_t)n * sizeof(double));
                gp = (double *)malloc((size_t)n * sizeof(double));
                gm = (double *)malloc((size_t)n * sizeof(double));
                if (!gp || !gm) {
                    free(xpj); free(gp); free(gm);
                    break;
                }
                xpj[b] = xp + h;
                obj->grad(xpj, n, gp, obj->ctx);
                ++run->gevals;
                xpj[b] = xp - h;
                obj->grad(xpj, n, gm, obj->ctx);
                ++run->gevals;
                for (a = 0; a < n; ++a)
                    H[a * n + b] = (gp[a] - gm[a]) / (2.0 * h);
                free(xpj); free(gp); free(gm);
            }
        }
        if (make_pd(H, n) != 0) {
            int a, b;
            for (a = 0; a < n; ++a)
                for (b = 0; b < n; ++b) H[a * n + b] = (a == b) ? 1.0 : 0.0;
        }
        if (mlab_cholesky(H, n, L) != 0) {
            int a, b;
            for (a = 0; a < n; ++a)
                for (b = 0; b < n; ++b) L[a * n + b] = (a == b) ? 1.0 : 0.0;
        }
        /* solve H p = -g */
        for (i = 0; i < n; ++i) g[i] = -g[i];
        mlab_cholesky_solve(L, n, g, p);
        for (i = 0; i < n; ++i) g[i] = -g[i];
        {
            int fe_alpha = 0;
            alpha = mlab_armijo_backtrack(obj, x, p, 1.0, 1e-4, 30, &fe_alpha);
            run->fevals += fe_alpha;
        }
        if (alpha <= 0) {
            run->iters = it + 1;
            run->status = 1;
            break;
        }
        mlab_vec_axpy(x, alpha, p, n);
        run->iters = it + 1;
    }
    free(g); free(H); free(L); free(p);
    return run->status == 0 ? 0 : 1;
}

/* Armijo backtracking, accurate feval count */
static double bfgs_alpha(const mlab_objective *obj, double *x, int n,
                         const double *p, const double *g, int *fe_out)
{
    const double c1 = 1e-4;
    double alpha = 1.0;
    double f0 = obj->f(x, n, obj->ctx);
    double dphi0 = mlab_dot(g, p, n);
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    int k, fe = 1;
    if (!xt) {
        if (fe_out) *fe_out += fe;
        return 1e-4;
    }
    if (dphi0 >= 0) {
        free(xt);
        if (fe_out) *fe_out += fe;
        return 1e-4;
    }
    for (k = 0; k < 40; ++k) {
        int i;
        double ft;
        for (i = 0; i < n; ++i) xt[i] = x[i] + alpha * p[i];
        ft = obj->f(xt, n, obj->ctx);
        ++fe;
        if (ft <= f0 + c1 * alpha * dphi0) {
            free(xt);
            if (fe_out) *fe_out += fe;
            return alpha;
        }
        alpha *= 0.5;
        if (alpha < 1e-18) break;
    }
    free(xt);
    if (fe_out) *fe_out += fe;
    return alpha > 0 ? alpha : 1e-8;
}

int mlab_opt_bfgs(const mlab_objective *obj, double *x, double tol,
                  int max_iter, mlab_opt_run *run)
{
    int n = obj->dim;
    size_t nn = (size_t)n * (size_t)n;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *g_old = (double *)malloc((size_t)n * sizeof(double));
    double *s = (double *)malloc((size_t)n * sizeof(double));
    double *y = (double *)malloc((size_t)n * sizeof(double));
    double *Hy = (double *)malloc((size_t)n * sizeof(double));
    double *p = (double *)malloc((size_t)n * sizeof(double));
    double *H = (double *)malloc(nn * sizeof(double));
    int it, i, j;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g || !g_old || !s || !y || !Hy || !p || !H) {
        free(g); free(g_old); free(s); free(y); free(Hy); free(p); free(H);
        run->status = -1;
        return -1;
    }
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
    run->status = 1;
    obj->grad(x, n, g, obj->ctx);
    ++run->gevals;
    for (it = 0; it < max_iter; ++it) {
        double gn, alpha, sy;
        run->f_final = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, run->f_final);
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        for (i = 0; i < n; ++i) {
            double sum = 0.0;
            for (j = 0; j < n; ++j) sum += H[i * n + j] * g[j];
            p[i] = -sum;
        }
        {
            double pgn = mlab_dot(p, g, n);
            if (pgn >= 0) {
                for (i = 0; i < n; ++i) p[i] = -g[i];
            }
        }
        mlab_vec_copy(g_old, g, n);
        alpha = bfgs_alpha(obj, x, n, p, g, &run->fevals);
        if (alpha <= 0) alpha = 1e-8;
        for (i = 0; i < n; ++i) s[i] = alpha * p[i];
        mlab_vec_axpy(x, 1.0, s, n);
        obj->grad(x, n, g, obj->ctx);
        ++run->gevals;
        for (i = 0; i < n; ++i) y[i] = g[i] - g_old[i];
        sy = mlab_dot(s, y, n);
        if (sy > 1e-12 * mlab_nrm2(s, n) * mlab_nrm2(y, n)) {
            double *tmp = (double *)malloc(nn * sizeof(double));
            if (tmp) {
                double yHy = 0.0;
                for (i = 0; i < n; ++i) {
                    double sum = 0.0;
                    for (j = 0; j < n; ++j) sum += H[i * n + j] * y[j];
                    Hy[i] = sum;
                }
                yHy = mlab_dot(y, Hy, n);
                for (i = 0; i < n; ++i) {
                    for (j = 0; j < n; ++j) {
                        tmp[i * n + j] = H[i * n + j]
                            - (Hy[i] * s[j] + s[i] * Hy[j]) / sy
                            + (1.0 + yHy / sy) * s[i] * s[j] / sy;
                    }
                }
                memcpy(H, tmp, nn * sizeof(double));
                free(tmp);
            }
        } else {
            for (i = 0; i < n; ++i)
                for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
        run->iters = it + 1;
    }
    free(g); free(g_old); free(s); free(y); free(Hy); free(p); free(H);
    return run->status == 0 ? 0 : 1;
}

int mlab_cg_solve(const double *A, int n, const double *b, double *x,
                  int max_iter, double tol, int *iters_out, double *res_out)
{
    double *r = (double *)malloc((size_t)n * sizeof(double));
    double *p = (double *)malloc((size_t)n * sizeof(double));
    double *Ap = (double *)malloc((size_t)n * sizeof(double));
    double rsold, bnrm;
    int it, i, j;
    if (!r || !p || !Ap) {
        free(r); free(p); free(Ap);
        return -1;
    }
    /* r = b - A x */
    for (i = 0; i < n; ++i) {
        double s = 0.0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
        r[i] = b[i] - s;
    }
    mlab_vec_copy(p, r, n);
    rsold = mlab_dot(r, r, n);
    bnrm = mlab_nrm2(b, n);
    if (bnrm < 1e-300) bnrm = 1.0;
    if (iters_out) *iters_out = 0;
    if (res_out) *res_out = sqrt(rsold) / bnrm;
    for (it = 0; it < max_iter && it < n + 5; ++it) {
        double alpha, rsnew;
        for (i = 0; i < n; ++i) {
            double s = 0.0;
            for (j = 0; j < n; ++j) s += A[i * n + j] * p[j];
            Ap[i] = s;
        }
        {
            double pAp = mlab_dot(p, Ap, n);
            if (fabs(pAp) < 1e-300) break;
            alpha = rsold / pAp;
        }
        mlab_vec_axpy(x, alpha, p, n);
        mlab_vec_axpy(r, -alpha, Ap, n);
        rsnew = mlab_dot(r, r, n);
        if (sqrt(rsnew) / bnrm < tol) {
            if (iters_out) *iters_out = it + 1;
            if (res_out) *res_out = sqrt(rsnew) / bnrm;
            free(r); free(p); free(Ap);
            return 0;
        }
        {
            double beta = rsnew / rsold;
            int k;
            for (k = 0; k < n; ++k) p[k] = r[k] + beta * p[k];
        }
        rsold = rsnew;
        if (iters_out) *iters_out = it + 1;
        if (res_out) *res_out = sqrt(rsnew) / bnrm;
    }
    if (res_out && *res_out < tol) {
        free(r); free(p); free(Ap);
        return 0;
    }
    free(r); free(p); free(Ap);
    return 1;
}

int mlab_opt_dogleg(const mlab_objective *obj, double *x, double tol,
                    int max_iter, double delta0, mlab_opt_run *run)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *H = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *L = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *pn = (double *)malloc((size_t)n * sizeof(double));
    double *pc = (double *)malloc((size_t)n * sizeof(double));
    double *xt = (double *)malloc((size_t)n * sizeof(double));
    double delta = delta0 > 0 ? delta0 : 1.0;
    int it, i, j;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g || !H || !L || !pn || !pc || !xt) {
        free(g); free(H); free(L); free(pn); free(pc); free(xt);
        run->status = -1;
        return -1;
    }
    run->status = 1;
    for (it = 0; it < max_iter; ++it) {
        double f0, gn, nrm_g, rho, predicted, fnew, dpn;
        obj->grad(x, n, g, obj->ctx);
        ++run->gevals;
        f0 = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, f0);
        run->f_final = f0;
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        if (obj->hess) obj->hess(x, n, H, obj->ctx);
        else {
            for (i = 0; i < n; ++i)
                for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
        if (make_pd(H, n) != 0) {
            for (i = 0; i < n; ++i)
                for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
        mlab_cholesky(H, n, L);
        for (i = 0; i < n; ++i) g[i] = -g[i];
        mlab_cholesky_solve(L, n, g, pn); /* Newton step */
        for (i = 0; i < n; ++i) g[i] = -g[i];
        nrm_g = gn;
        /* Cauchy step: pc = - (g·g)/(g·Hg) g, or -delta g/||g|| */
        {
            double gHg = 0.0;
            for (i = 0; i < n; ++i) {
                double s = 0.0;
                for (j = 0; j < n; ++j) s += H[i * n + j] * g[j];
                gHg += g[i] * s;
            }
            if (gHg > 1e-300) {
                double tau = (nrm_g * nrm_g) / gHg;
                for (i = 0; i < n; ++i) pc[i] = -tau * g[i];
            } else {
                for (i = 0; i < n; ++i) pc[i] = 0.0;
            }
        }
        dpn = mlab_nrm2(pn, n);
        /* dogleg path */
        if (dpn <= delta) {
            mlab_vec_copy(xt, pn, n);
        } else if (mlab_nrm2(pc, n) >= delta) {
            double npc = mlab_nrm2(pc, n);
            for (i = 0; i < n; ++i) xt[i] = (delta / npc) * pc[i];
        } else {
            /* find tau in [0,1]: ||pc + tau (pn-pc)|| = delta */
            double a = 0, b = 0, c = 0, disc, tau;
            for (i = 0; i < n; ++i) {
                double di = pn[i] - pc[i];
                a += di * di;
                b += 2.0 * pc[i] * di;
                c += pc[i] * pc[i];
            }
            c -= delta * delta;
            disc = b * b - 4 * a * c;
            if (disc < 0 || a < 1e-300) {
                mlab_vec_copy(xt, pc, n);
            } else {
                tau = (-b + sqrt(disc)) / (2 * a);
                if (tau < 0) tau = 0;
                if (tau > 1) tau = 1;
                for (i = 0; i < n; ++i) xt[i] = pc[i] + tau * (pn[i] - pc[i]);
            }
        }
        for (i = 0; i < n; ++i) xt[i] = x[i] + xt[i];
        fnew = obj->f(xt, n, obj->ctx);
        ++run->fevals;
        /* predicted reduction ≈ -g·p - 0.5 p H p */
        {
            double *pvec = (double *)malloc((size_t)n * sizeof(double));
            predicted = 0.0;
            if (pvec) {
                for (i = 0; i < n; ++i) {
                    pvec[i] = xt[i] - x[i];
                    predicted += -g[i] * pvec[i];
                }
                for (i = 0; i < n; ++i) {
                    double s = 0.0;
                    for (j = 0; j < n; ++j) s += H[i * n + j] * pvec[j];
                    predicted -= 0.5 * pvec[i] * s;
                }
                free(pvec);
            }
            if (predicted < 1e-300) predicted = 1e-300;
        }
        rho = (f0 - fnew) / predicted;
        if (rho < 0.25) delta *= 0.5;
        else if (rho > 0.75 && dpn > 0.9 * delta) delta *= 2.0;
        if (rho > 1e-4) {
            mlab_vec_copy(x, xt, n);
        }
        run->iters = it + 1;
        if (delta < 1e-14 && rho <= 1e-4) {
            /* stalled */
            run->status = (gn < tol) ? 0 : 1;
            break;
        }
    }
    free(g); free(H); free(L); free(pn); free(pc); free(xt);
    return run->status == 0 ? 0 : 1;
}

int mlab_opt_neldermead(const mlab_objective *obj, double *x, double tol,
                        int max_iter, mlab_opt_run *run)
{
    const int n = obj->dim;
    const double rho = 1.0, chi = 2.0, psi = 0.5, sigma = 0.5;
    double *simplex = (double *)malloc((size_t)(n + 1) * (size_t)n * sizeof(double));
    double *fv = (double *)malloc((size_t)(n + 1) * sizeof(double));
    double *xg = (double *)malloc((size_t)n * sizeof(double));
    double *xr = (double *)malloc((size_t)n * sizeof(double));
    double *xe = (double *)malloc((size_t)n * sizeof(double));
    double *xc = (double *)malloc((size_t)n * sizeof(double));
    int it, i, j;
    mlab_opt_run local;
    double best_f;
    double *x_best = NULL;
    if (!run) run = &local;
    run_reset(run);
    if (!simplex || !fv || !xg || !xr || !xe || !xc) {
        free(simplex); free(fv); free(xg); free(xr); free(xe); free(xc);
        run->status = -1;
        return -1;
    }
    x_best = (double *)malloc((size_t)n * sizeof(double));
    /* initial simplex */
    for (j = 0; j < n; ++j) simplex[0 * n + j] = x[j];
    fv[0] = obj->f(x, n, obj->ctx);
    ++run->fevals;
    best_f = fv[0];
    if (x_best) mlab_vec_copy(x_best, x, n);
    for (i = 1; i <= n; ++i) {
        for (j = 0; j < n; ++j) simplex[i * n + j] = x[j];
        simplex[i * n + (i - 1)] += 0.25 * (fabs(x[i - 1]) + 1.0);
        fv[i] = obj->f(simplex + i * n, n, obj->ctx);
        ++run->fevals;
        if (fv[i] < best_f) {
            best_f = fv[i];
            if (x_best) mlab_vec_copy(x_best, simplex + i * n, n);
        }
    }
    run->status = 1;
    for (it = 0; it < max_iter; ++it) {
        int best = 0, worst = 0, second = 0;
        double fbest, fworst, fsecond, fr, fe, fc, fstd;
        for (i = 0; i <= n; ++i) {
            if (fv[i] < fv[best]) best = i;
            if (fv[i] > fv[worst]) worst = i;
        }
        second = (best == 0) ? 1 : 0;
        for (i = 0; i <= n; ++i) {
            if (i == worst) continue;
            if (fv[i] > fv[second] || second == worst) second = i;
        }
        fbest = fv[best];
        fworst = fv[worst];
        fsecond = fv[second];
        hist_push(run, it, fbest);
        run->f_final = fbest;
        /* std of fv as convergence proxy */
        {
            double mu = 0.0, v = 0.0;
            for (i = 0; i <= n; ++i) mu += fv[i];
            mu /= (n + 1);
            for (i = 0; i <= n; ++i) v += (fv[i] - mu) * (fv[i] - mu);
            fstd = sqrt(v / (n + 1));
        }
        run->gnorm = fstd; /* not gradient; used as diversity */
        if (fstd < tol) {
            run->iters = it + 1;
            run->status = 0;
            mlab_vec_copy(x, simplex + best * n, n);
            break;
        }
        /* centroid excluding worst */
        for (j = 0; j < n; ++j) xg[j] = 0.0;
        for (i = 0; i <= n; ++i) {
            if (i == worst) continue;
            for (j = 0; j < n; ++j) xg[j] += simplex[i * n + j];
        }
        for (j = 0; j < n; ++j) xg[j] /= n;
        /* reflect */
        for (j = 0; j < n; ++j) xr[j] = xg[j] + rho * (xg[j] - simplex[worst * n + j]);
        fr = obj->f(xr, n, obj->ctx);
        ++run->fevals;
        if (fr < best_f) {
            best_f = fr;
            if (x_best) mlab_vec_copy(x_best, xr, n);
        }
        if (fr < fbest) {
            for (j = 0; j < n; ++j) xe[j] = xg[j] + chi * (xr[j] - xg[j]);
            fe = obj->f(xe, n, obj->ctx);
            ++run->fevals;
            if (fe < best_f) {
                best_f = fe;
                if (x_best) mlab_vec_copy(x_best, xe, n);
            }
            if (fe < fr) {
                mlab_vec_copy(simplex + worst * n, xe, n);
                fv[worst] = fe;
            } else {
                mlab_vec_copy(simplex + worst * n, xr, n);
                fv[worst] = fr;
            }
        } else if (fr < fsecond) {
            mlab_vec_copy(simplex + worst * n, xr, n);
            fv[worst] = fr;
        } else {
            int outside = (fr < fworst);
            if (outside) {
                for (j = 0; j < n; ++j) xc[j] = xg[j] + psi * (xr[j] - xg[j]);
            } else {
                for (j = 0; j < n; ++j) xc[j] = xg[j] - psi * (xg[j] - simplex[worst * n + j]);
            }
            fc = obj->f(xc, n, obj->ctx);
            ++run->fevals;
            if (fc < best_f) {
                best_f = fc;
                if (x_best) mlab_vec_copy(x_best, xc, n);
            }
            if (fc < (outside ? fr : fworst)) {
                mlab_vec_copy(simplex + worst * n, xc, n);
                fv[worst] = fc;
            } else {
                /* shrink toward best */
                for (i = 0; i <= n; ++i) {
                    if (i == best) continue;
                    for (j = 0; j < n; ++j)
                        simplex[i * n + j] = simplex[best * n + j] +
                            sigma * (simplex[i * n + j] - simplex[best * n + j]);
                    fv[i] = obj->f(simplex + i * n, n, obj->ctx);
                    ++run->fevals;
                    if (fv[i] < best_f) {
                        best_f = fv[i];
                        if (x_best) mlab_vec_copy(x_best, simplex + i * n, n);
                    }
                }
            }
        }
        run->iters = it + 1;
    }
    /* final: best-ever under noise, else simplex best */
    if (x_best) {
        mlab_vec_copy(x, x_best, n);
        run->f_final = best_f;
        free(x_best);
    } else {
        int best = 0;
        for (i = 1; i <= n; ++i) if (fv[i] < fv[best]) best = i;
        mlab_vec_copy(x, simplex + best * n, n);
        run->f_final = fv[best];
    }
    free(simplex); free(fv); free(xg); free(xr); free(xe); free(xc);
    return run->status == 0 ? 0 : 1;
}

/* ---- L-BFGS（双循环递推，mem 个 (s,y) 对） ---- */

int mlab_opt_lbfgs(const mlab_objective *obj, double *x, double tol,
                   int max_iter, int mem, mlab_opt_run *run)
{
    int n = obj->dim;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *g_old = (double *)malloc((size_t)n * sizeof(double));
    double *p = (double *)malloc((size_t)n * sizeof(double));
    double *q = (double *)malloc((size_t)n * sizeof(double));
    double *r = (double *)malloc((size_t)n * sizeof(double));
    double *sbuf = (double *)malloc((size_t)mem * n * sizeof(double));
    double *ybuf = (double *)malloc((size_t)mem * n * sizeof(double));
    double *rho = (double *)malloc((size_t)mem * sizeof(double));
    double *al = (double *)malloc((size_t)mem * sizeof(double));
    int it, i, j, count = 0;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g || !g_old || !p || !q || !r || !sbuf || !ybuf || !rho || !al || mem <= 0) {
        free(g); free(g_old); free(p); free(q); free(r);
        free(sbuf); free(ybuf); free(rho); free(al);
        run->status = -1;
        return -1;
    }
    run->status = 1;
    obj->grad(x, n, g, obj->ctx);
    ++run->gevals;
    for (it = 0; it < max_iter; ++it) {
        double gn, alpha, sy;
        run->f_final = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, run->f_final);
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        /* 双循环：q = g；逆方向遍历 (s,y) */
        mlab_vec_copy(q, g, n);
        for (j = count - 1; j >= 0; --j) {
            const double *sj = sbuf + (size_t)j * n;
            const double *yj = ybuf + (size_t)j * n;
            double saq = 0.0;
            for (i = 0; i < n; ++i) saq += sj[i] * q[i];
            al[j] = saq / rho[j];
            for (i = 0; i < n; ++i) q[i] -= al[j] * yj[i];
        }
        /* H0 = gamma I, gamma = sy_last / yy_last */
        if (count > 0) {
            const double *s0 = sbuf + (size_t)(count - 1) * n;
            const double *y0 = ybuf + (size_t)(count - 1) * n;
            double sy = 0.0, yy = 0.0;
            for (i = 0; i < n; ++i) {
                sy += s0[i] * y0[i];
                yy += y0[i] * y0[i];
            }
            {
                double gamma = (yy > 0) ? sy / yy : 1.0;
                if (!(gamma > 0)) gamma = 1.0;
                for (i = 0; i < n; ++i) r[i] = gamma * q[i];
            }
        } else {
            mlab_vec_copy(r, q, n);
        }
        for (j = 0; j < count; ++j) {
            const double *sj = sbuf + (size_t)j * n;
            const double *yj = ybuf + (size_t)j * n;
            double yar = 0.0, beta;
            for (i = 0; i < n; ++i) yar += yj[i] * r[i];
            beta = yar / rho[j];
            for (i = 0; i < n; ++i) r[i] += sj[i] * (al[j] - beta);
        }
        for (i = 0; i < n; ++i) p[i] = -r[i];
        {
            /* 数值保护：方向非下降则退回最速下降 */
            double pgn = mlab_dot(p, g, n);
            if (!(pgn < 0)) {
                for (i = 0; i < n; ++i) p[i] = -g[i];
            }
        }
        mlab_vec_copy(g_old, g, n);
        alpha = bfgs_alpha(obj, x, n, p, g, &run->fevals);
        if (alpha <= 0) {
            run->iters = it + 1;
            run->status = 1;
            break;
        }
        {
            double *snew = sbuf + (size_t)count * n;
            double *ynew = ybuf + (size_t)count * n;
            for (i = 0; i < n; ++i) {
                snew[i] = alpha * p[i];
                x[i] += snew[i];
            }
            obj->grad(x, n, g, obj->ctx);
            ++run->gevals;
            sy = 0.0;
            for (i = 0; i < n; ++i) {
                ynew[i] = g[i] - g_old[i];
                sy += snew[i] * ynew[i];
            }
            {
                /* 曲率条件用相对阈值：sy <= 0 或过小的 (s,y) 对不可用；
                   此时清空缓冲退回 H0=gamma·I（最速下降方向）重建近似，
                   避免"冻结的旧近似 + 非下降对"造成的停滞 */
                double sns = mlab_nrm2(snew, n);
                double syn = mlab_nrm2(ynew, n);
                if (sy > 1e-10 * sns * syn && sy > 0.0) {
                    rho[count] = sy;
                    count = (count + 1) % mem == 0 ? mem : count + 1;
                    if (count > mem) count = mem;
                    if (count == mem) {
                        /* 环形淘汰最旧一对：整体前移 */
                        memmove(sbuf, sbuf + n, (size_t)(mem - 1) * n * sizeof(double));
                        memmove(ybuf, ybuf + n, (size_t)(mem - 1) * n * sizeof(double));
                        memmove(rho, rho + 1, (size_t)(mem - 1) * sizeof(double));
                        count = mem - 1;
                    }
                } else {
                    count = 0;
                }
            }
        }
        run->iters = it + 1;
    }
    free(g); free(g_old); free(p); free(q); free(r);
    free(sbuf); free(ybuf); free(rho); free(al);
    return run->status == 0 ? 0 : 1;
}

/* ---- DFP（H+ = H - Hy y^T H / y^T H y + s s^T / s^T y） ---- */

int mlab_opt_dfp(const mlab_objective *obj, double *x, double tol,
                 int max_iter, mlab_opt_run *run)
{
    int n = obj->dim;
    size_t nn = (size_t)n * (size_t)n;
    double *g = (double *)malloc((size_t)n * sizeof(double));
    double *g_old = (double *)malloc((size_t)n * sizeof(double));
    double *s = (double *)malloc((size_t)n * sizeof(double));
    double *y = (double *)malloc((size_t)n * sizeof(double));
    double *Hy = (double *)malloc((size_t)n * sizeof(double));
    double *p = (double *)malloc((size_t)n * sizeof(double));
    double *H = (double *)malloc(nn * sizeof(double));
    double *tmp = (double *)malloc(nn * sizeof(double));
    int it, i, j;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!g || !g_old || !s || !y || !Hy || !p || !H || !tmp) {
        free(g); free(g_old); free(s); free(y); free(Hy); free(p); free(H); free(tmp);
        run->status = -1;
        return -1;
    }
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
    run->status = 1;
    obj->grad(x, n, g, obj->ctx);
    ++run->gevals;
    for (it = 0; it < max_iter; ++it) {
        double gn, alpha, sy, yHy;
        run->f_final = obj->f(x, n, obj->ctx);
        ++run->fevals;
        hist_push(run, it, run->f_final);
        gn = mlab_nrm2(g, n);
        run->gnorm = gn;
        if (gn < tol) {
            run->iters = it + 1;
            run->status = 0;
            break;
        }
        for (i = 0; i < n; ++i) {
            double sum = 0.0;
            for (j = 0; j < n; ++j) sum += H[i * n + j] * g[j];
            p[i] = -sum;
        }
        mlab_vec_copy(g_old, g, n);
        alpha = bfgs_alpha(obj, x, n, p, g, &run->fevals);
        if (alpha <= 0) {
            run->iters = it + 1;
            run->status = 1;
            break;
        }
        for (i = 0; i < n; ++i) s[i] = alpha * p[i];
        mlab_vec_axpy(x, 1.0, s, n);
        obj->grad(x, n, g, obj->ctx);
        ++run->gevals;
        for (i = 0; i < n; ++i) y[i] = g[i] - g_old[i];
        sy = mlab_dot(s, y, n);
        if (sy > 1e-12 * mlab_nrm2(s, n) * mlab_nrm2(y, n)) {
            for (i = 0; i < n; ++i) {
                double sum = 0.0;
                for (j = 0; j < n; ++j) sum += H[i * n + j] * y[j];
                Hy[i] = sum;
            }
            yHy = mlab_dot(y, Hy, n);
            if (yHy > 1e-300) {
                /* H - Hy Hy^T / yHy + s s^T / sy */
                for (i = 0; i < n; ++i)
                    for (j = 0; j < n; ++j)
                        tmp[i * n + j] = H[i * n + j] - Hy[i] * Hy[j] / yHy
                                         + s[i] * s[j] / sy;
                memcpy(H, tmp, nn * sizeof(double));
            }
        } else {
            for (i = 0; i < n; ++i)
                for (j = 0; j < n; ++j) H[i * n + j] = (i == j) ? 1.0 : 0.0;
        }
        run->iters = it + 1;
    }
    free(g); free(g_old); free(s); free(y); free(Hy); free(p); free(H); free(tmp);
    return run->status == 0 ? 0 : 1;
}

/* ---- 坐标下降（每坐标黄金分割精确一维最小化） ---- */

typedef struct {
    const mlab_objective *obj;
    double *x;
    int j, n;
    int *fe;
} cd_ctx;

static double cd_phi(double alpha, void *vctx)
{
    cd_ctx *c = (cd_ctx *)vctx;
    double save = c->x[c->j];
    double f;
    c->x[c->j] = save + alpha;
    f = c->obj->f(c->x, c->n, c->obj->ctx);
    ++(*c->fe);
    c->x[c->j] = save;
    return f;
}

int mlab_opt_coord_descent(const mlab_objective *obj, double *x, double tol,
                           int max_sweeps, double bracket, mlab_opt_run *run)
{
    int n = obj->dim;
    int it, i;
    double f_prev;
    int fe = 0;
    cd_ctx cctx;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (n <= 0 || !(bracket > 0.0)) {
        run->status = -1;
        return -1;
    }
    run->status = 1;
    cctx.obj = obj;
    cctx.x = x;
    cctx.n = n;
    cctx.fe = &fe;
    f_prev = obj->f(x, n, obj->ctx);
    ++fe;
    for (it = 0; it < max_sweeps; ++it) {
        for (i = 0; i < n; ++i) {
            double dx;
            cctx.j = i; /* 指定当前坐标（此前缺失导致扰动错误坐标） */
            dx = mlab_golden_section(cd_phi, &cctx, -bracket, bracket,
                                     100, 1e-12, NULL, NULL);
            if (dx > bracket) dx = bracket;
            if (dx < -bracket) dx = -bracket;
            x[i] += dx;
        }
        run->f_final = obj->f(x, n, obj->ctx);
        ++fe;
        hist_push(run, it, run->f_final);
        run->iters = it + 1;
        if (fabs(f_prev - run->f_final) < tol) {
            run->status = 0;
            break;
        }
        f_prev = run->f_final;
    }
    run->fevals = fe;
    run->gnorm = 0.0;
    return run->status == 0 ? 0 : 1;
}

/* ---- Hooke-Jeeves（探测移动 + 模式移动 + 步长减半） ---- */

int mlab_opt_hooke_jeeves(const mlab_objective *obj, double *x, double tol,
                          int max_iter, double step0, mlab_opt_run *run)
{
    int n = obj->dim;
    double *x_prev = (double *)malloc((size_t)n * sizeof(double));
    double step = step0 > 0 ? step0 : 1.0;
    double f_base;
    int it, i;
    mlab_opt_run local;
    if (!run) run = &local;
    run_reset(run);
    if (!x_prev) {
        run->status = -1;
        return -1;
    }
    run->status = 1;
    f_base = obj->f(x, n, obj->ctx);
    ++run->fevals;
    for (it = 0; it < max_iter && step > tol * 0.1; ++it) {
        int improved = 0;
        mlab_vec_copy(x_prev, x, n);
        /* 探测移动：逐坐标 ±step */
        for (i = 0; i < n; ++i) {
            double f_try;
            x[i] += step;
            f_try = obj->f(x, n, obj->ctx);
            ++run->fevals;
            if (f_try < f_base) {
                f_base = f_try;
                improved = 1;
                continue;
            }
            x[i] -= 2.0 * step;
            f_try = obj->f(x, n, obj->ctx);
            ++run->fevals;
            if (f_try < f_base) {
                f_base = f_try;
                improved = 1;
                continue;
            }
            x[i] += step; /* 还原 */
        }
        /* 模式移动：沿 (x - x_prev) 再走一步 */
        if (improved) {
            double f_try;
            for (i = 0; i < n; ++i) x[i] += (x[i] - x_prev[i]);
            f_try = obj->f(x, n, obj->ctx);
            ++run->fevals;
            if (f_try >= f_base) {
                for (i = 0; i < n; ++i) x[i] -= (x[i] - x_prev[i]) * 0.5;
            } else {
                f_base = f_try;
            }
        } else {
            step *= 0.5;
        }
        run->f_final = f_base;
        hist_push(run, it, f_base);
        run->iters = it + 1;
    }
    run->gnorm = step;
    free(x_prev);
    return run->status == 0 ? 0 : 1;
}

/* ---- DIRECT（分割超矩形，单维划分变体） ---- */

typedef struct {
    double *centers; /* cap * n */
    double *sides;   /* cap * n 每维边长 */
    double *f;
    int count, cap, n;
} dbox_pool;

static int dp_init(dbox_pool *p, int cap, int n)
{
    p->centers = (double *)malloc((size_t)cap * n * sizeof(double));
    p->sides = (double *)malloc((size_t)cap * n * sizeof(double));
    p->f = (double *)malloc((size_t)cap * sizeof(double));
    p->count = 0;
    p->cap = cap;
    p->n = n;
    return (p->centers && p->sides && p->f) ? 0 : -1;
}

static void dp_free(dbox_pool *p)
{
    free(p->centers);
    free(p->sides);
    free(p->f);
}

int mlab_direct(const mlab_objective *obj, const double *lb, const double *ub,
                int n, int max_evals, double side_tol,
                double *x_best_out, double *f_best_out, int *n_eval_out)
{
    dbox_pool pool = {NULL, NULL, NULL, 0, 0, 0};
    double *trial = (double *)malloc((size_t)n * sizeof(double));
    double f_best;
    int i, d, ne = 0, status = 1;
    if (!trial || dp_init(&pool, 4096, n) != 0) {
        free(trial);
        dp_free(&pool);
        return -1;
    }
    for (d = 0; d < n; ++d) {
        pool.centers[d] = 0.5 * (lb[d] + ub[d]);
        pool.sides[d] = ub[d] - lb[d];
    }
    pool.f[0] = obj->f(pool.centers, n, obj->ctx);
    ++ne;
    pool.count = 1;
    f_best = pool.f[0];
    while (ne < max_evals && pool.count > 0) {
        int no_split = 0;
        /* 按最长边升序做下左凸包扫描 → 潜在最优盒 */
        int *order = (int *)malloc((size_t)pool.count * sizeof(int));
        char *pot = (char *)calloc((size_t)pool.count, 1);
        int npot = 0, bi;
        if (!order || !pot) {
            free(order);
            free(pot);
            break;
        }
        for (i = 0; i < pool.count; ++i) order[i] = i;
        for (i = 1; i < pool.count; ++i) {
            int oi = order[i], j = i - 1;
            double si = 0.0, sj;
            for (d = 0; d < n; ++d) if (pool.sides[oi * n + d] > si) si = pool.sides[oi * n + d];
            while (j >= 0) {
                int oj = order[j];
                sj = 0.0;
                for (d = 0; d < n; ++d) if (pool.sides[oj * n + d] > sj) sj = pool.sides[oj * n + d];
                if (sj > si) {
                    order[j + 1] = order[j];
                    --j;
                } else break;
            }
            order[j + 1] = oi;
        }
        {
            double best_f = 1e300;
            for (i = 0; i < pool.count; ++i) {
                int idx = order[i];
                if (pool.f[idx] < best_f - 1e-15) {
                    pot[idx] = 1;
                    ++npot;
                    best_f = pool.f[idx];
                }
            }
            if (npot == 0) {
                bi = 0;
                for (i = 1; i < pool.count; ++i) if (pool.f[i] < pool.f[bi]) bi = i;
                pot[bi] = 1;
            }
        }
        free(order);
        /*
         * 经典 DIRECT 划分：对每个潜在最优盒，取全部"最宽"维（至多 3 维，
         * 防止组合爆炸）同时三分；评估除中心外全部 3^w-1 个子盒中心，
         * 生成 3^w 个子盒（中心子盒沿用父盒 f）。
         */
        no_split = 0;
        {
            const int max_children = 27; /* 3^3 */
            int added = 0;
            double *ncent = (double *)malloc((size_t)npot * max_children * n * sizeof(double));
            double *nside = (double *)malloc((size_t)npot * max_children * n * sizeof(double));
            double *nf = (double *)malloc((size_t)npot * max_children * sizeof(double));
            int *remove_idx = (int *)malloc((size_t)npot * sizeof(int));
            int *wdims = (int *)malloc((size_t)n * sizeof(int));
            int *nch_arr = (int *)malloc((size_t)npot * sizeof(int));
            if (!ncent || !nside || !nf || !remove_idx || !wdims || !nch_arr) {
                free(ncent); free(nside); free(nf); free(remove_idx); free(wdims);
                free(nch_arr);
                free(pot);
                break;
            }
            for (bi = 0; bi < pool.count && added < npot && ne < max_evals; ++bi) {
                int nw = 0, nchild, ci;
                double side_long = 0.0;
                double *cpar = pool.centers + (size_t)bi * n;
                if (!pot[bi]) continue;
                for (d = 0; d < n; ++d) {
                    if (pool.sides[bi * n + d] > side_long) {
                        side_long = pool.sides[bi * n + d];
                        nw = 1;
                        wdims[0] = d;
                    } else if (pool.sides[bi * n + d] == side_long && nw < 3) {
                        wdims[nw++] = d; /* 最宽维并列（上限 3 维） */
                    }
                }
                if (side_long < side_tol) continue;
                nchild = 1;
                for (d = 0; d < nw; ++d) nchild *= 3;
                /* 评估除中心外全部子盒中心：offset 三进制位 ∈ {-1,0,1}^w */
                for (ci = 0; ci < nchild && ne < max_evals; ++ci) {
                    int c0 = ci, any_nz = 0;
                    double ft;
                    for (i = 0; i < n; ++i) trial[i] = cpar[i];
                    for (d = 0; d < nw; ++d) {
                        int o3 = c0 % 3;
                        c0 /= 3;
                        if (o3 != 0) any_nz = 1;
                        trial[wdims[d]] += (o3 - 1) * pool.sides[bi * n + wdims[d]] / 3.0;
                    }
                    if (!any_nz) continue; /* 中心子盒：f 沿用父盒 */
                    ft = obj->f(trial, n, obj->ctx);
                    ++ne;
                    for (i = 0; i < n; ++i) {
                        ncent[(size_t)added * max_children * n + (size_t)ci * n + i] = trial[i];
                        nside[(size_t)added * max_children * n + (size_t)ci * n + i] =
                            pool.sides[bi * n + i];
                    }
                    for (d = 0; d < nw; ++d)
                        nside[(size_t)added * max_children * n + (size_t)ci * n + wdims[d]] =
                            pool.sides[bi * n + wdims[d]] / 3.0;
                    nf[(size_t)added * max_children + ci] = ft;
                    if (ft < f_best) f_best = ft;
                }
                /* 中心子盒（ci=0 的槽位：offset 全 0） */
                for (i = 0; i < n; ++i) {
                    ncent[(size_t)added * max_children * n + i] = cpar[i];
                    nside[(size_t)added * max_children * n + i] = pool.sides[bi * n + i];
                }
                for (d = 0; d < nw; ++d)
                    nside[(size_t)added * max_children * n + wdims[d]] =
                        pool.sides[bi * n + wdims[d]] / 3.0;
                nf[(size_t)added * max_children + 0] = pool.f[bi];
                remove_idx[added] = bi;
                nch_arr[added] = nchild; /* 只追加实际生成的 3^w 个子盒 */
                ++added;
            }
            if (added == 0) no_split = 1; /* 无可划分的潜在最优盒 */
            {
                int k;
                for (k = added - 1; k >= 0; --k) {
                    int idx = remove_idx[k];
                    if (idx != pool.count - 1) {
                        memmove(pool.centers + (size_t)idx * n,
                                pool.centers + (size_t)(idx + 1) * n,
                                (size_t)(pool.count - 1 - idx) * n * sizeof(double));
                        memmove(pool.sides + (size_t)idx * n,
                                pool.sides + (size_t)(idx + 1) * n,
                                (size_t)(pool.count - 1 - idx) * n * sizeof(double));
                        memmove(pool.f + idx, pool.f + idx + 1,
                                (size_t)(pool.count - 1 - idx) * sizeof(double));
                    }
                    --pool.count;
                }
            }
            for (i = 0; i < added; ++i) {
                int c;
                for (c = 0; c < nch_arr[i] && pool.count < pool.cap; ++c) {
                    memcpy(pool.centers + (size_t)pool.count * n,
                           ncent + ((size_t)i * max_children + c) * n,
                           (size_t)n * sizeof(double));
                    memcpy(pool.sides + (size_t)pool.count * n,
                           nside + ((size_t)i * max_children + c) * n,
                           (size_t)n * sizeof(double));
                    pool.f[pool.count] = nf[(size_t)i * max_children + c];
                    if (pool.f[pool.count] < f_best) f_best = pool.f[pool.count];
                    ++pool.count;
                }
            }
            free(ncent);
            free(nside);
            free(nf);
            free(remove_idx);
            free(wdims);
            free(nch_arr);
        }
        free(pot);
        if (no_split) {
            status = 0; /* 所有潜在最优盒已足够小 → 收敛 */
            break;
        }
        {
            double max_side = 0.0;
            for (i = 0; i < pool.count; ++i) {
                for (d = 0; d < n; ++d)
                    if (pool.sides[i * n + d] > max_side) max_side = pool.sides[i * n + d];
            }
            if (pool.count == 0 || max_side < side_tol) {
                status = 0;
                break;
            }
        }
    }
    if (pool.count > 0) {
        int bi = 0;
        for (i = 1; i < pool.count; ++i)
            if (pool.f[i] < pool.f[bi]) bi = i;
        if (f_best_out) *f_best_out = pool.f[bi];
        memcpy(x_best_out, pool.centers + (size_t)bi * n, (size_t)n * sizeof(double));
    }
    if (n_eval_out) *n_eval_out = ne;
    dp_free(&pool);
    free(trial);
    return status;
}

