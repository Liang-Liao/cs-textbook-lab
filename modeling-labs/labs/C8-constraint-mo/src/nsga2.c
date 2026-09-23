#include "nsga2.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_nsga2_result_free(mlab_nsga2_result *r)
{
    if (!r) return;
    free(r->X);
    free(r->F);
    free(r->rank);
    free(r->crowd);
    free(r->igd_hist);
    r->X = r->F = r->crowd = r->igd_hist = NULL;
    r->rank = NULL;
}

void mlab_zdt1_eval(const double *x, int n, double *f, int nobj, void *ctx)
{
    double g, s = 0.0, ratio;
    int i;
    (void)ctx;
    if (nobj < 2 || n < 2) return;
    f[0] = x[0];
    for (i = 1; i < n; ++i) s += x[i];
    g = 1.0 + 9.0 * s / (double)(n - 1);
    if (g < 1e-12) g = 1e-12;
    ratio = f[0] / g;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    f[1] = g * (1.0 - sqrt(ratio));
}

void mlab_zdt1_true_pf(double *f1, double *f2, int npts)
{
    int i;
    for (i = 0; i < npts; ++i) {
        double a = (npts <= 1) ? 0.0 : (double)i / (double)(npts - 1);
        f1[i] = a;
        f2[i] = 1.0 - sqrt(a);
    }
}

double mlab_igd(const double *ref, int nref, const double *set, int np, int nobj)
{
    int i, j, k;
    double sum = 0.0;
    if (!ref || !set || nref <= 0 || np <= 0 || nobj <= 0) return 1e300;
    for (i = 0; i < nref; ++i) {
        double best = 1e300;
        for (j = 0; j < np; ++j) {
            double d = 0.0;
            for (k = 0; k < nobj; ++k) {
                double df = ref[i * nobj + k] - set[j * nobj + k];
                d += df * df;
            }
            if (d < best) best = d;
        }
        sum += sqrt(best);
    }
    return sum / (double)nref;
}

double mlab_hv2d(const double *set, int np, double ref_f1, double ref_f2)
{
    double *f1, *f2, hv = 0.0;
    int i, nnd = 0, *idx;

    if (!set || np <= 0) return 0.0;
    f1 = (double *)malloc((size_t)np * sizeof(double));
    f2 = (double *)malloc((size_t)np * sizeof(double));
    idx = (int *)malloc((size_t)np * sizeof(int));
    if (!f1 || !f2 || !idx) {
        free(f1); free(f2); free(idx);
        return 0.0;
    }
    for (i = 0; i < np; ++i) idx[i] = i;
    for (i = 1; i < np; ++i) {
        int key = idx[i], j = i - 1;
        while (j >= 0 && set[idx[j] * 2] > set[key * 2]) {
            idx[j + 1] = idx[j];
            --j;
        }
        idx[j + 1] = key;
    }
    for (i = 0; i < np; ++i) {
        double a = set[idx[i] * 2], b = set[idx[i] * 2 + 1];
        if (a >= ref_f1 || b >= ref_f2) continue;
        if (nnd == 0 || b < f2[nnd - 1] - 1e-15) {
            f1[nnd] = a;
            f2[nnd] = b;
            ++nnd;
        }
    }
    for (i = 0; i < nnd; ++i) {
        double next_f1 = (i + 1 < nnd) ? f1[i + 1] : ref_f1;
        double w = next_f1 - f1[i];
        double h = ref_f2 - f2[i];
        if (w > 0 && h > 0) hv += w * h;
    }
    free(f1); free(f2); free(idx);
    return hv;
}

static int dom_ab(const double *fa, const double *fb, int nobj)
{
    /* 1: a dominates b; -1: b dominates a; 0: incomparable */
    int i, a_better = 0, b_better = 0;
    for (i = 0; i < nobj; ++i) {
        if (fa[i] < fb[i] - 1e-14) a_better = 1;
        if (fb[i] < fa[i] - 1e-14) b_better = 1;
    }
    if (a_better && !b_better) return 1;
    if (b_better && !a_better) return -1;
    return 0;
}

/* 就地非支配排序：rank[0..n-1]，未入前沿者为 -1 */
static void nd_sort(const double *F, int n, int nobj, int *rank, int *dom_cnt)
{
    int i, j, r, remaining;
    int *S; /* n x n: 个体 i 支配的列表 */
    int *Sc;

    for (i = 0; i < n; ++i) {
        rank[i] = -1;
        dom_cnt[i] = 0;
    }
    S = (int *)malloc((size_t)n * n * sizeof(int));
    Sc = (int *)calloc((size_t)n, sizeof(int));
    if (!S || !Sc) { free(S); free(Sc); return; }

    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            int d;
            if (i == j) continue;
            d = dom_ab(&F[i * nobj], &F[j * nobj], nobj);
            if (d == 1) {
                S[i * n + Sc[i]++] = j;
                ++dom_cnt[j];
            }
        }
    }

    remaining = n;
    r = 0;
    while (remaining > 0) {
        int *cur = (int *)malloc((size_t)n * sizeof(int));
        int nc = 0, k;
        if (!cur) break;
        for (i = 0; i < n; ++i)
            if (dom_cnt[i] == 0 && rank[i] < 0) {
                cur[nc++] = i;
                rank[i] = r;
            }
        if (nc == 0) {
            /* 环/数值问题：剩余全标为当前 rank */
            for (i = 0; i < n; ++i)
                if (rank[i] < 0) { rank[i] = r; ++remaining; }
            /* 已全部处理 */
            for (i = 0; i < n; ++i)
                if (rank[i] < 0) rank[i] = r;
            free(cur);
            break;
        }
        for (k = 0; k < nc; ++k) {
            int ii = cur[k], t;
            for (t = 0; t < Sc[ii]; ++t) {
                int jj = S[ii * n + t];
                if (dom_cnt[jj] > 0) --dom_cnt[jj];
            }
            --remaining;
        }
        free(cur);
        ++r;
        if (r > n + 2) break;
    }
    for (i = 0; i < n; ++i)
        if (rank[i] < 0) rank[i] = r;
    free(S);
    free(Sc);
}

static void crowding_dist_front(const double *F, const int *idx, int n,
                                int nobj, double *crowd)
{
    int i, j, o, *ord;
    double *tmpF;
    if (n <= 0) return;
    if (n == 1) { crowd[idx[0]] = 1e300; return; }
    ord = (int *)malloc((size_t)n * sizeof(int));
    tmpF = (double *)malloc((size_t)n * nobj * sizeof(double));
    if (!ord || !tmpF) { free(ord); free(tmpF); return; }
    for (i = 0; i < n; ++i) {
        crowd[idx[i]] = 0.0;
        ord[i] = i;
        for (o = 0; o < nobj; ++o)
            tmpF[i * nobj + o] = F[idx[i] * nobj + o];
    }
    for (o = 0; o < nobj; ++o) {
        double fmin, fmax, span;
        for (i = 0; i < n; ++i) ord[i] = i;
        for (i = 1; i < n; ++i) {
            int key = ord[i];
            j = i - 1;
            while (j >= 0 && tmpF[ord[j] * nobj + o] > tmpF[key * nobj + o]) {
                ord[j + 1] = ord[j];
                --j;
            }
            ord[j + 1] = key;
        }
        crowd[idx[ord[0]]] = 1e300;
        crowd[idx[ord[n - 1]]] = 1e300;
        fmin = tmpF[ord[0] * nobj + o];
        fmax = tmpF[ord[n - 1] * nobj + o];
        span = fmax - fmin;
        if (span <= 0.0) span = 1.0;
        for (i = 1; i < n - 1; ++i) {
            int id = idx[ord[i]];
            if (crowd[id] >= 1e200) continue;
            crowd[id] +=
                (tmpF[ord[i + 1] * nobj + o] - tmpF[ord[i - 1] * nobj + o]) / span;
        }
    }
    free(ord);
    free(tmpF);
}

static void crowding_all_fronts(const double *F, int n, int nobj,
                                const int *rank, double *crowd)
{
    int r, i, maxr = 0;
    int *idx;
    for (i = 0; i < n; ++i) {
        crowd[i] = 0.0;
        if (rank[i] > maxr) maxr = rank[i];
    }
    idx = (int *)malloc((size_t)n * sizeof(int));
    if (!idx) return;
    for (r = 0; r <= maxr; ++r) {
        int nf = 0;
        for (i = 0; i < n; ++i)
            if (rank[i] == r) idx[nf++] = i;
        crowding_dist_front(F, idx, nf, nobj, crowd);
    }
    free(idx);
}

static void sbx_one(mlab_rng *rng, const double *p1, const double *p2,
                    double *c, int dim, const double *lb, const double *ub,
                    double p_cross, double eta_c, double p_mut, double eta_m)
{
    int d;
    for (d = 0; d < dim; ++d) {
        double y1 = p1[d], y2 = p2[d];
        if (mlab_rng_uniform(rng) < p_cross) {
            double u = mlab_rng_uniform(rng);
            double beta;
            if (u <= 0.5)
                beta = pow(2.0 * u, 1.0 / (eta_c + 1.0));
            else
                beta = pow(1.0 / (2.0 * (1.0 - u + 1e-16)), 1.0 / (eta_c + 1.0));
            c[d] = 0.5 * ((1.0 + beta) * y1 + (1.0 - beta) * y2);
        } else {
            c[d] = y1;
        }
        if (mlab_rng_uniform(rng) < p_mut) {
            double lo = lb ? lb[d] : 0.0;
            double hi = ub ? ub[d] : 1.0;
            double y = c[d];
            double u = mlab_rng_uniform(rng);
            if (u < 0.5) {
                double delta = pow(2.0 * u, 1.0 / (eta_m + 1.0)) - 1.0;
                y = y + delta * (y - lo);
            } else {
                double delta = 1.0 - pow(2.0 * (1.0 - u), 1.0 / (eta_m + 1.0));
                y = y + delta * (hi - y);
            }
            c[d] = y;
        }
        if (lb && c[d] < lb[d]) c[d] = lb[d];
        if (ub && c[d] > ub[d]) c[d] = ub[d];
    }
}

static void sort_idx_by_crowd(int *idx, int n, const double *crowd)
{
    int i, j;
    for (i = 1; i < n; ++i) {
        int key = idx[i];
        j = i - 1;
        while (j >= 0 && crowd[idx[j]] < crowd[key]) {
            idx[j + 1] = idx[j];
            --j;
        }
        idx[j + 1] = key;
    }
}

int mlab_nsga2_run(mlab_rng *rng, const mlab_nsga2_config *cfg,
                   const double *igd_ref, int nref,
                   mlab_nsga2_result *out)
{
    int pop, dim, nobj, g, gen, i, d;
    double *X, *F, *cX, *cF, *crowd;
    int *rank, *dom_cnt;
    int hist_cap;
    double p_cross, eta_c, p_mut, eta_m;

    if (!rng || !cfg || !cfg->eval || !out || cfg->pop < 4 ||
        cfg->dim < 2 || cfg->nobj != 2)
        return 0;
    pop = cfg->pop;
    dim = cfg->dim;
    nobj = cfg->nobj;
    p_cross = cfg->p_cross > 0 ? cfg->p_cross : 0.9;
    eta_c = cfg->eta_c > 0 ? cfg->eta_c : 20.0;
    p_mut = cfg->p_mut > 0 ? cfg->p_mut : 1.0 / (double)dim;
    eta_m = cfg->eta_m > 0 ? cfg->eta_m : 20.0;
    gen = cfg->max_gen > 0 ? cfg->max_gen : 100;

    memset(out, 0, sizeof *out);
    out->pop = pop;
    out->dim = dim;
    out->nobj = nobj;
    hist_cap = gen + 2;

    X = (double *)malloc((size_t)pop * dim * sizeof(double));
    F = (double *)malloc((size_t)pop * nobj * sizeof(double));
    cX = (double *)malloc((size_t)pop * dim * sizeof(double));
    cF = (double *)malloc((size_t)pop * nobj * sizeof(double));
    crowd = (double *)malloc((size_t)pop * sizeof(double));
    rank = (int *)malloc((size_t)pop * sizeof(int));
    dom_cnt = (int *)malloc((size_t)pop * sizeof(int));
    out->X = (double *)malloc((size_t)pop * dim * sizeof(double));
    out->F = (double *)malloc((size_t)pop * nobj * sizeof(double));
    out->rank = (int *)malloc((size_t)pop * sizeof(int));
    out->crowd = (double *)malloc((size_t)pop * sizeof(double));
    if (igd_ref && nref > 0)
        out->igd_hist = (double *)malloc((size_t)hist_cap * sizeof(double));
    if (!X || !F || !cX || !cF || !crowd || !rank || !dom_cnt ||
        !out->X || !out->F || !out->rank || !out->crowd ||
        (igd_ref && nref > 0 && !out->igd_hist)) {
        free(X); free(F); free(cX); free(cF); free(crowd);
        free(rank); free(dom_cnt);
        mlab_nsga2_result_free(out);
        return 0;
    }

    for (i = 0; i < pop; ++i) {
        for (d = 0; d < dim; ++d) {
            double lo = cfg->lb ? cfg->lb[d] : 0.0;
            double hi = cfg->ub ? cfg->ub[d] : 1.0;
            X[i * dim + d] = lo + (hi - lo) * mlab_rng_uniform(rng);
        }
        cfg->eval(&X[i * dim], dim, &F[i * nobj], nobj, cfg->ctx);
        ++out->n_eval;
    }

    for (g = 0; g <= gen; ++g) {
        int N2, taken, r;
        double *MX, *MF, *Mc;
        int *Mrank, *Mcnt, *order;

        nd_sort(F, pop, nobj, rank, dom_cnt);
        crowding_all_fronts(F, pop, nobj, rank, crowd);
        if (igd_ref && nref > 0 && out->igd_len < hist_cap)
            out->igd_hist[out->igd_len++] = mlab_igd(igd_ref, nref, F, pop, nobj);
        if (g == gen) break;

        /* 变异：DE/rand/1/bin（C5 机制）+ 少量 SBX，保证 g 可被压低 */
        for (i = 0; i < pop; ++i) {
            int r1, r2, r3, jrand, d;
            double Fde = 0.5, CRde = 0.9;
            do { r1 = (int)(mlab_rng_uniform(rng) * pop) % pop; } while (r1 == i);
            do { r2 = (int)(mlab_rng_uniform(rng) * pop) % pop; } while (r2 == i || r2 == r1);
            do { r3 = (int)(mlab_rng_uniform(rng) * pop) % pop; } while (r3 == i || r3 == r1 || r3 == r2);
            jrand = (int)(mlab_rng_uniform(rng) * dim) % dim;
            for (d = 0; d < dim; ++d) {
                double v = X[r1 * dim + d] + Fde * (X[r2 * dim + d] - X[r3 * dim + d]);
                if (!(mlab_rng_uniform(rng) < CRde) && d != jrand)
                    v = X[i * dim + d];
                if (cfg->lb && v < cfg->lb[d]) v = cfg->lb[d];
                if (cfg->ub && v > cfg->ub[d]) v = cfg->ub[d];
                cX[i * dim + d] = v;
            }
            /* 10% 概率再用 SBX 扰动一对父代，保持 NSGA-II 交叉成分 */
            if (mlab_rng_uniform(rng) < 0.10) {
                int a = (int)(mlab_rng_uniform(rng) * pop) % pop;
                int b = (int)(mlab_rng_uniform(rng) * pop) % pop;
                sbx_one(rng, &X[a * dim], &X[b * dim], &cX[i * dim], dim,
                        cfg->lb, cfg->ub, p_cross, eta_c, p_mut, eta_m);
            }
            cfg->eval(&cX[i * dim], dim, &cF[i * nobj], nobj, cfg->ctx);
            ++out->n_eval;
        }

        /* 精英：P∪Q 选 pop */
        N2 = pop * 2;
        MX = (double *)malloc((size_t)N2 * dim * sizeof(double));
        MF = (double *)malloc((size_t)N2 * nobj * sizeof(double));
        Mc = (double *)malloc((size_t)N2 * sizeof(double));
        Mrank = (int *)malloc((size_t)N2 * sizeof(int));
        Mcnt = (int *)malloc((size_t)N2 * sizeof(int));
        order = (int *)malloc((size_t)N2 * sizeof(int));
        if (!MX || !MF || !Mc || !Mrank || !Mcnt || !order) {
            free(MX); free(MF); free(Mc); free(Mrank); free(Mcnt); free(order);
            break;
        }
        memcpy(MX, X, (size_t)pop * dim * sizeof(double));
        memcpy(MF, F, (size_t)pop * nobj * sizeof(double));
        memcpy(MX + pop * dim, cX, (size_t)pop * dim * sizeof(double));
        memcpy(MF + pop * nobj, cF, (size_t)pop * nobj * sizeof(double));
        nd_sort(MF, N2, nobj, Mrank, Mcnt);
        crowding_all_fronts(MF, N2, nobj, Mrank, Mc);

        taken = 0;
        for (r = 0; r <= pop && taken < pop; ++r) {
            int nf = 0;
            for (i = 0; i < N2; ++i)
                if (Mrank[i] == r) order[nf++] = i;
            if (nf == 0) continue;
            if (taken + nf <= pop) {
                for (i = 0; i < nf; ++i) {
                    int src = order[i];
                    memcpy(&X[taken * dim], &MX[src * dim], (size_t)dim * sizeof(double));
                    memcpy(&F[taken * nobj], &MF[src * nobj], (size_t)nobj * sizeof(double));
                    ++taken;
                }
            } else {
                sort_idx_by_crowd(order, nf, Mc);
                for (i = 0; i < nf && taken < pop; ++i) {
                    int src = order[i];
                    memcpy(&X[taken * dim], &MX[src * dim], (size_t)dim * sizeof(double));
                    memcpy(&F[taken * nobj], &MF[src * nobj], (size_t)nobj * sizeof(double));
                    ++taken;
                }
            }
        }
        free(MX); free(MF); free(Mc); free(Mrank); free(Mcnt); free(order);
        out->gen_used = g + 1;
    }

    nd_sort(F, pop, nobj, rank, dom_cnt);
    crowding_all_fronts(F, pop, nobj, rank, crowd);
    memcpy(out->X, X, (size_t)pop * dim * sizeof(double));
    memcpy(out->F, F, (size_t)pop * nobj * sizeof(double));
    memcpy(out->rank, rank, (size_t)pop * sizeof(int));
    memcpy(out->crowd, crowd, (size_t)pop * sizeof(double));
    out->gen_used = gen;

    free(X); free(F); free(cX); free(cF); free(crowd);
    free(rank); free(dom_cnt);
    return 1;
}
