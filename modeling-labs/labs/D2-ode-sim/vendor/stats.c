#include "stats.h"

#include <math.h>
#include <stdlib.h>

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

double mlab_mean(const double *x, int n)
{
    double s = 0.0;
    int i;
    if (n <= 0) return 0.0;
    for (i = 0; i < n; ++i) s += x[i];
    return s / n;
}

double mlab_var(const double *x, int n)
{
    double mu, s = 0.0;
    int i;
    if (n < 2) return 0.0;
    mu = mlab_mean(x, n);
    for (i = 0; i < n; ++i) {
        double d = x[i] - mu;
        s += d * d;
    }
    return s / (n - 1);
}

double mlab_std(const double *x, int n)
{
    double v = mlab_var(x, n);
    return v > 0 ? sqrt(v) : 0.0;
}

double mlab_cov(const double *x, const double *y, int n)
{
    double mx, my, s = 0.0;
    int i;
    if (n < 2) return 0.0;
    mx = mlab_mean(x, n);
    my = mlab_mean(y, n);
    for (i = 0; i < n; ++i) s += (x[i] - mx) * (y[i] - my);
    return s / (n - 1);
}

void mlab_sort_asc(double *x, int n)
{
    if (n > 1) qsort(x, (size_t)n, sizeof(double), cmp_double);
}

double mlab_ecdf(const double *xs_sorted, int n, double x)
{
    int lo = 0, hi = n, mid;
    if (n <= 0) return 0.0;
    /* count how many <= x */
    while (lo < hi) {
        mid = lo + (hi - lo) / 2;
        if (xs_sorted[mid] <= x) lo = mid + 1;
        else hi = mid;
    }
    return (double)lo / (double)n;
}

void mlab_mle_normal(const double *x, int n, double *mu_out, double *sigma_out)
{
    double mu, s = 0.0;
    int i;
    if (n <= 0) {
        if (mu_out) *mu_out = 0.0;
        if (sigma_out) *sigma_out = 0.0;
        return;
    }
    mu = mlab_mean(x, n);
    for (i = 0; i < n; ++i) {
        double d = x[i] - mu;
        s += d * d;
    }
    if (mu_out) *mu_out = mu;
    if (sigma_out) *sigma_out = sqrt(s / n); /* MLE：1/n */
}

int mlab_ci_mean_z(const double *x, int n, double sigma, double z,
                   double *lo_out, double *hi_out)
{
    double xbar, half;
    if (n <= 0 || !x || !(sigma > 0.0) || !(z > 0.0)) return -1;
    xbar = mlab_mean(x, n);
    half = z * sigma / sqrt((double)n);
    if (lo_out) *lo_out = xbar - half;
    if (hi_out) *hi_out = xbar + half;
    return 0;
}
