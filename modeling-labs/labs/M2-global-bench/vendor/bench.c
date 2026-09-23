#include "bench.h"

#include <string.h>

double mlab_sphere(const double *x, int n, void *ctx)
{
    double s = 0.0;
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) s += x[i] * x[i];
    return 0.5 * s;
}

void mlab_sphere_grad(const double *x, int n, double *g, void *ctx)
{
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) g[i] = x[i];
}

void mlab_sphere_hess(const double *x, int n, double *H, void *ctx)
{
    int i, j;
    (void)x;
    (void)ctx;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j)
            H[i * n + j] = (i == j) ? 1.0 : 0.0;
}

double mlab_rosenbrock(const double *x, int n, void *ctx)
{
    double s = 0.0;
    int i;
    (void)ctx;
    for (i = 0; i + 1 < n; ++i) {
        double a = x[i + 1] - x[i] * x[i];
        double b = 1.0 - x[i];
        s += 100.0 * a * a + b * b;
    }
    return s;
}

void mlab_rosenbrock_grad(const double *x, int n, double *g, void *ctx)
{
    int i;
    (void)ctx;
    for (i = 0; i < n; ++i) g[i] = 0.0;
    for (i = 0; i + 1 < n; ++i) {
        double a = x[i + 1] - x[i] * x[i];
        g[i]     += -400.0 * x[i] * a - 2.0 * (1.0 - x[i]);
        g[i + 1] += 200.0 * a;
    }
}

void mlab_rosenbrock_hess(const double *x, int n, double *H, void *ctx)
{
    int i, j;
    (void)ctx;
    memset(H, 0, (size_t)n * (size_t)n * sizeof(double));
    for (i = 0; i + 1 < n; ++i) {
        double a = x[i + 1] - x[i] * x[i];
        H[i * n + i]       += -400.0 * a + 800.0 * x[i] * x[i] + 2.0;
        H[i * n + (i + 1)] += -400.0 * x[i];
        H[(i + 1) * n + i] += -400.0 * x[i];
        H[(i + 1) * n + (i + 1)] += 200.0;
    }
    (void)j;
}

double mlab_quad(const double *x, int n, void *ctx)
{
    const mlab_quad_ctx *q = (const mlab_quad_ctx *)ctx;
    const double *A = q->A;
    double s = 0.0;
    int i, j;
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) s += x[i] * A[i * n + j] * x[j];
    }
    return 0.5 * s;
}

void mlab_quad_grad(const double *x, int n, double *g, void *ctx)
{
    const mlab_quad_ctx *q = (const mlab_quad_ctx *)ctx;
    const double *A = q->A;
    int i, j;
    for (i = 0; i < n; ++i) {
        double s = 0.0;
        for (j = 0; j < n; ++j) s += A[i * n + j] * x[j];
        g[i] = s;
    }
}

void mlab_quad_hess(const double *x, int n, double *H, void *ctx)
{
    const mlab_quad_ctx *q = (const mlab_quad_ctx *)ctx;
    memcpy(H, q->A, (size_t)n * (size_t)n * sizeof(double));
    (void)x;
}
