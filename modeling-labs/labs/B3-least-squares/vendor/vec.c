#include "vec.h"

#include <math.h>

double mlab_dot(const double *a, const double *b, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; ++i) s += a[i] * b[i];
    return s;
}

double mlab_nrm2(const double *x, int n)
{
    return sqrt(mlab_dot(x, x, n));
}

double mlab_nrm1(const double *x, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; ++i) s += fabs(x[i]);
    return s;
}

double mlab_nrminf(const double *x, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; ++i) {
        double a = fabs(x[i]);
        if (a > s) s = a;
    }
    return s;
}

double mlab_dist2(const double *a, const double *b, int n)
{
    double s = 0.0;
    int i;
    for (i = 0; i < n; ++i) {
        double d = a[i] - b[i];
        s += d * d;
    }
    return sqrt(s);
}

void mlab_vec_zero(double *x, int n)
{
    int i;
    for (i = 0; i < n; ++i) x[i] = 0.0;
}

void mlab_vec_copy(double *dst, const double *src, int n)
{
    int i;
    for (i = 0; i < n; ++i) dst[i] = src[i];
}

void mlab_vec_axpy(double y[], double a, const double *x, int n)
{
    int i;
    for (i = 0; i < n; ++i) y[i] += a * x[i];
}

void mlab_vec_scale(double x[], double a, int n)
{
    int i;
    for (i = 0; i < n; ++i) x[i] *= a;
}
