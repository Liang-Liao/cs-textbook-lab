#include "numiter.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int inputs_ok(const double *A, int n, const double *b, const double *x)
{
    int i;
    if (n <= 0 || !A || !b || !x) return 0;
    for (i = 0; i < n; ++i)
        if (A[i * (size_t)n + i] == 0.0) return 0;
    return 1;
}

int mlab_jacobi_solve(const double *A, int n, const double *b, double *x,
                      int max_iter, double tol, int *iters_out)
{
    double *nx;
    int it, i, j, rc = 1;
    if (!inputs_ok(A, n, b, x)) return -1;
    if (iters_out) *iters_out = 0;
    if (max_iter <= 0 || !(tol >= 0.0)) return -1;
    nx = (double *)malloc((size_t)n * sizeof(double));
    if (!nx) return -1;
    for (it = 0; it < max_iter; ++it) {
        double step = 0.0;
        for (i = 0; i < n; ++i) {
            double s = b[i];
            for (j = 0; j < n; ++j)
                if (j != i) s -= A[i * (size_t)n + j] * x[j];
            nx[i] = s / A[i * (size_t)n + i];
            if (fabs(nx[i] - x[i]) > step) step = fabs(nx[i] - x[i]);
        }
        memcpy(x, nx, (size_t)n * sizeof(double));
        if (iters_out) *iters_out = it + 1;
        if (step < tol) {
            rc = 0;
            break;
        }
    }
    free(nx);
    return rc;
}

int mlab_gauss_seidel_solve(const double *A, int n, const double *b, double *x,
                            int max_iter, double tol, int *iters_out)
{
    int it, i, j, rc = 1;
    if (!inputs_ok(A, n, b, x)) return -1;
    if (max_iter <= 0 || !(tol >= 0.0)) return -1;
    for (it = 0; it < max_iter; ++it) {
        double step = 0.0;
        for (i = 0; i < n; ++i) {
            double s = b[i];
            for (j = 0; j < i; ++j) s -= A[i * (size_t)n + j] * x[j]; /* 已更新 */
            for (j = i + 1; j < n; ++j) s -= A[i * (size_t)n + j] * x[j];
            {
                double xi_new = s / A[i * (size_t)n + i];
                if (fabs(xi_new - x[i]) > step) step = fabs(xi_new - x[i]);
                x[i] = xi_new;
            }
        }
        if (iters_out) *iters_out = it + 1;
        if (step < tol) {
            rc = 0;
            break;
        }
    }
    return rc;
}

int mlab_sor_solve(const double *A, int n, const double *b, double omega, double *x,
                   int max_iter, double tol, int *iters_out)
{
    int it, i, j, rc = 1;
    if (!inputs_ok(A, n, b, x)) return -1;
    if (max_iter <= 0 || !(tol >= 0.0) || !(omega > 0.0)) return -1;
    for (it = 0; it < max_iter; ++it) {
        double step = 0.0;
        for (i = 0; i < n; ++i) {
            double s = b[i];
            for (j = 0; j < i; ++j) s -= A[i * (size_t)n + j] * x[j];
            for (j = i + 1; j < n; ++j) s -= A[i * (size_t)n + j] * x[j];
            {
                double gs = s / A[i * (size_t)n + i];
                double xi_new = (1.0 - omega) * x[i] + omega * gs;
                if (fabs(xi_new - x[i]) > step) step = fabs(xi_new - x[i]);
                x[i] = xi_new;
            }
        }
        if (iters_out) *iters_out = it + 1;
        if (step < tol) {
            rc = 0;
            break;
        }
    }
    return rc;
}

/* deterministic xorshift32（与 linalg.c 同款，起始向量可复现） */
static unsigned long xorshift32(unsigned long *s)
{
    unsigned long v = *s;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    *s = v & 0xFFFFFFFFul;
    return v;
}

double mlab_jacobi_spectral_radius(const double *A, int n)
{
    double *v, *w;
    unsigned long s = 0x1234567ul;
    double rho = -1.0;
    int i, j, it;
    if (n <= 0 || !A) return -1.0;
    for (i = 0; i < n; ++i)
        if (A[i * (size_t)n + i] == 0.0) return -1.0;
    v = (double *)malloc((size_t)n * sizeof(double));
    w = (double *)malloc((size_t)n * sizeof(double));
    if (!v || !w) {
        free(v);
        free(w);
        return -1.0;
    }
    for (i = 0; i < n; ++i)
        v[i] = (double)(xorshift32(&s) % 2001 - 1000) / 1000.0;
    {
        double nv = 0.0;
        for (i = 0; i < n; ++i) nv += v[i] * v[i];
        nv = sqrt(nv);
        if (nv < 1e-300) nv = 1.0;
        for (i = 0; i < n; ++i) v[i] /= nv;
    }
    for (it = 0; it < 500; ++it) {
        double nw = 0.0;
        for (i = 0; i < n; ++i) {
            double aii = A[i * (size_t)n + i];
            double s = 0.0;
            for (j = 0; j < n; ++j)
                if (j != i) s += A[i * (size_t)n + j] * v[j];
            w[i] = -s / aii; /* M v 的第 i 分量 = -(sum_{j!=i} a_ij v_j)/a_ii */
        }
        for (i = 0; i < n; ++i) nw += w[i] * w[i];
        nw = sqrt(nw);
        if (nw < 1e-300) {
            rho = 0.0;
            break;
        }
        rho = nw; /* M 对称时收敛到谱半径 */
        for (i = 0; i < n; ++i) v[i] = w[i] / nw;
    }
    free(v);
    free(w);
    return rho;
}
