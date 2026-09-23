#include "linalg.h"
#include "vec.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* deterministic xorshift32 */
static unsigned long xorshift32(unsigned long *s)
{
    unsigned long x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x & 0xFFFFFFFFul;
    return *s;
}

static double urand(unsigned long *s)
{
    return (xorshift32(s) + 0.5) / 4294967296.0;
}

static double grand(unsigned long *s)
{
    /* Box-Muller */
    double u1 = urand(s), u2 = urand(s);
    if (u1 < 1e-300) u1 = 1e-300;
    return sqrt(-2.0 * log(u1)) * cos(6.283185307179586 * u2);
}

int mlab_lu_factor(double *lu, int n, int *piv)
{
    int i, j, k, p;
    if (n <= 0 || !lu || !piv) return -1;
    for (i = 0; i < n; ++i) piv[i] = i;
    for (k = 0; k < n; ++k) {
        double amax = fabs(lu[k * n + k]);
        p = k;
        for (i = k + 1; i < n; ++i) {
            double v = fabs(lu[i * n + k]);
            if (v > amax) {
                amax = v;
                p = i;
            }
        }
        if (amax == 0.0) return -1;
        if (p != k) {
            int tmp = piv[k];
            piv[k] = piv[p];
            piv[p] = tmp;
            for (j = 0; j < n; ++j) {
                double t = lu[k * n + j];
                lu[k * n + j] = lu[p * n + j];
                lu[p * n + j] = t;
            }
        }
        {
            double diag = lu[k * n + k];
            for (i = k + 1; i < n; ++i) {
                double m = lu[i * n + k] / diag;
                lu[i * n + k] = m;
                if (m != 0.0) {
                    for (j = k + 1; j < n; ++j)
                        lu[i * n + j] -= m * lu[k * n + j];
                }
            }
        }
    }
    return 0;
}

int mlab_lu_solve(const double *lu, int n, const int *piv, double *x, const double *b)
{
    int i, j;
    double *y;
    if (n <= 0 || !lu || !piv || !x || !b) return -1;
    y = (double *)malloc((size_t)n * sizeof(double));
    if (!y) return -1;
    /* P b */
    for (i = 0; i < n; ++i) y[i] = b[piv[i]];
    /* L y = Pb (unit L, stored below diag) */
    for (i = 0; i < n; ++i) {
        for (j = 0; j < i; ++j) y[i] -= lu[i * n + j] * y[j];
    }
    /* U x = y */
    for (i = n - 1; i >= 0; --i) {
        double s = y[i];
        for (j = i + 1; j < n; ++j) s -= lu[i * n + j] * x[j];
        x[i] = s / lu[i * n + i];
    }
    free(y);
    return 0;
}

int mlab_lu_solve_dense(const double *A, int n, const double *b, double *x)
{
    double *lu;
    int *piv;
    int rc;
    size_t nn = (size_t)n * (size_t)n;
    if (n <= 0) return -1;
    lu = (double *)malloc(nn * sizeof(double));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (!lu || !piv) {
        free(lu);
        free(piv);
        return -1;
    }
    memcpy(lu, A, nn * sizeof(double));
    rc = mlab_lu_factor(lu, n, piv);
    if (rc == 0) rc = mlab_lu_solve(lu, n, piv, x, b);
    free(lu);
    free(piv);
    return rc;
}

int mlab_cholesky(const double *A, int n, double *l_out)
{
    int i, j, k;
    if (n <= 0 || !A || !l_out) return -1;
    memset(l_out, 0, (size_t)n * (size_t)n * sizeof(double));
    for (i = 0; i < n; ++i) {
        for (j = 0; j <= i; ++j) {
            double s = A[i * n + j];
            for (k = 0; k < j; ++k) s -= l_out[i * n + k] * l_out[j * n + k];
            if (i == j) {
                if (s <= 0.0) return -1;
                l_out[i * n + j] = sqrt(s);
            } else {
                l_out[i * n + j] = s / l_out[j * n + j];
            }
        }
    }
    return 0;
}

int mlab_cholesky_solve(const double *l, int n, const double *b, double *x)
{
    int i, j;
    double *y;
    if (n <= 0) return -1;
    y = (double *)malloc((size_t)n * sizeof(double));
    if (!y) return -1;
    for (i = 0; i < n; ++i) {
        double s = b[i];
        for (j = 0; j < i; ++j) s -= l[i * n + j] * y[j];
        y[i] = s / l[i * n + i];
    }
    for (i = n - 1; i >= 0; --i) {
        double s = y[i];
        for (j = i + 1; j < n; ++j) s -= l[j * n + i] * x[j];
        x[i] = s / l[i * n + i];
    }
    free(y);
    return 0;
}

/* power iteration for largest eig of M (symmetric), returns lambda_max */
static double power_max_eig(const double *M, int n, unsigned long *seed)
{
    double *v = (double *)malloc((size_t)n * sizeof(double));
    double *w = (double *)malloc((size_t)n * sizeof(double));
    double lam = 0.0;
    int it, i, j;
    if (!v || !w) {
        free(v);
        free(w);
        return -1.0;
    }
    memset(v, 0, (size_t)n * sizeof(double));
    for (i = 0; i < n; ++i) v[i] = grand(seed);
    {
        double nv = mlab_nrm2(v, n);
        if (nv < 1e-300) nv = 1.0;
        for (i = 0; i < n; ++i) v[i] /= nv;
    }
    for (it = 0; it < 200; ++it) {
        for (i = 0; i < n; ++i) {
            double s = 0.0;
            for (j = 0; j < n; ++j) s += M[i * n + j] * v[j];
            w[i] = s;
        }
        lam = mlab_dot(v, w, n);
        {
            double nw = mlab_nrm2(w, n);
            if (nw < 1e-300) break;
            for (i = 0; i < n; ++i) v[i] = w[i] / nw;
        }
    }
    free(v);
    free(w);
    return lam;
}

static int matmul_nt(double *C, const double *A, const double *B, int n, int trans_b)
{
    /* C = A * B or A * B^T , all n x n */
    int i, j, k;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (k = 0; k < n; ++k) {
                double a = A[i * n + k];
                double b = trans_b ? B[j * n + k] : B[k * n + j];
                s += a * b;
            }
            C[i * n + j] = s;
        }
    }
    return 0;
}

double mlab_cond2_estimate(const double *A, int n)
{
    double *lu = 0, *ata = 0, *inv = 0, *col = 0, *e = 0, *x = 0;
    int *piv = 0;
    unsigned long seed = 0x9E3779B9ul;
    double smax, smin, cond;
    int i, j;

    lu = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    ata = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    inv = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    col = (double *)malloc((size_t)n * sizeof(double));
    e = (double *)malloc((size_t)n * sizeof(double));
    x = (double *)malloc((size_t)n * sizeof(double));
    piv = (int *)malloc((size_t)n * sizeof(int));
    if (!lu || !ata || !inv || !col || !e || !x || !piv) goto fail;

    memcpy(lu, A, (size_t)n * (size_t)n * sizeof(double));
    if (mlab_lu_factor(lu, n, piv) != 0) goto fail;
    for (j = 0; j < n; ++j) {
        for (i = 0; i < n; ++i) e[i] = 0.0;
        e[j] = 1.0;
        if (mlab_lu_solve(lu, n, piv, x, e) != 0) goto fail;
        for (i = 0; i < n; ++i) inv[i * n + j] = x[i];
    }
    /* A^T A */
    matmul_nt(ata, A, A, n, 1);
    smax = power_max_eig(ata, n, &seed);
    if (smax <= 0.0) goto fail;
    /* (A^{-1})(A^{-1})^T  eigenvalues = 1/sigma^2 */
    matmul_nt(ata, inv, inv, n, 1);
    {
        double inv_lmax = power_max_eig(ata, n, &seed);
        if (inv_lmax <= 0.0) goto fail;
        smin = 1.0 / sqrt(inv_lmax);
        smax = sqrt(smax);
    }
    if (smin <= 0.0) goto fail;
    cond = smax / smin;

    free(lu);
    free(ata);
    free(inv);
    free(col);
    free(e);
    free(x);
    free(piv);
    return cond;

fail:
    free(lu);
    free(ata);
    free(inv);
    free(col);
    free(e);
    free(x);
    free(piv);
    return -1.0;
}

double mlab_spd_cond2(const double *A, int n)
{
    double *ata = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    unsigned long seed = 0xA5A5A5A5ul;
    double lmax, lmin, cond;
    /* inverse via Cholesky */
    double *l = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *inv = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *e = (double *)malloc((size_t)n * sizeof(double));
    double *x = (double *)malloc((size_t)n * sizeof(double));
    int i, j;
    if (!ata || !l || !inv || !e || !x) {
        free(ata);
        free(l);
        free(inv);
        free(e);
        free(x);
        return -1.0;
    }
    if (mlab_cholesky(A, n, l) != 0) {
        free(ata);
        free(l);
        free(inv);
        free(e);
        free(x);
        return -1.0;
    }
    for (j = 0; j < n; ++j) {
        for (i = 0; i < n; ++i) e[i] = 0.0;
        e[j] = 1.0;
        mlab_cholesky_solve(l, n, e, x);
        for (i = 0; i < n; ++i) inv[i * n + j] = x[i];
    }
    memcpy(ata, A, (size_t)n * (size_t)n * sizeof(double));
    lmax = power_max_eig(ata, n, &seed);
    matmul_nt(ata, inv, inv, n, 0);
    {
        double invmax = power_max_eig(ata, n, &seed);
        lmin = 1.0 / invmax; /* for SPD, eig(A^{-1}) max = 1/lambda_min(A) */
        /* wait: power on inv*inv^T = A^{-2} gives 1/lambda_min(A)^2 */
        lmin = 1.0 / sqrt(invmax);
    }
    /* For SPD, power on A gives lambda_max; power on A^{-2} gives 1/lambda_min^2 */
    if (lmax <= 0 || lmin <= 0) {
        free(ata);
        free(l);
        free(inv);
        free(e);
        free(x);
        return -1.0;
    }
    cond = lmax / lmin;
    free(ata);
    free(l);
    free(inv);
    free(e);
    free(x);
    return cond;
}

/* Modified Gram-Schmidt on random matrix -> orthogonal Q */
int mlab_random_orthogonal(double *Q, int n, unsigned seed)
{
    unsigned long s = seed ? seed : 1ul;
    int i, j, k;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            Q[i * n + j] = grand(&s);
    for (j = 0; j < n; ++j) {
        for (k = 0; k < j; ++k) {
            double d = 0.0;
            for (i = 0; i < n; ++i) d += Q[i * n + k] * Q[i * n + j];
            for (i = 0; i < n; ++i) Q[i * n + j] -= d * Q[i * n + k];
        }
        {
            double nrm = 0.0;
            for (i = 0; i < n; ++i) nrm += Q[i * n + j] * Q[i * n + j];
            nrm = sqrt(nrm);
            if (nrm < 1e-12) return -1;
            for (i = 0; i < n; ++i) Q[i * n + j] /= nrm;
        }
    }
    return 0;
}

int mlab_spd_from_spectrum(double *A, int n, const double *evals, unsigned seed)
{
    double *Q = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    int i, j, k;
    if (!Q) return -1;
    if (mlab_random_orthogonal(Q, n, seed) != 0) {
        free(Q);
        return -1;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (k = 0; k < n; ++k) s += Q[i * n + k] * evals[k] * Q[j * n + k];
            A[i * n + j] = s;
        }
    }
    free(Q);
    return 0;
}

int mlab_mat_from_svd_spectrum(double *A, int n, const double *sigma, unsigned seed)
{
    double *Q1 = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *Q2 = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *tmp = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    int i, j, k;
    if (!Q1 || !Q2 || !tmp) {
        free(Q1);
        free(Q2);
        free(tmp);
        return -1;
    }
    if (mlab_random_orthogonal(Q1, n, seed) != 0 ||
        mlab_random_orthogonal(Q2, n, seed + 9973u) != 0) {
        free(Q1);
        free(Q2);
        free(tmp);
        return -1;
    }
    /* tmp = Q1 * diag(sigma)  (scale columns of Q1) */
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            tmp[i * n + j] = Q1[i * n + j] * sigma[j];
    /* A = tmp * Q2^T */
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double s = 0.0;
            for (k = 0; k < n; ++k) s += tmp[i * n + k] * Q2[j * n + k];
            A[i * n + j] = s;
        }
    }
    free(Q1);
    free(Q2);
    free(tmp);
    return 0;
}
