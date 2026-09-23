#include "numcal.h"

#include <math.h>

double mlab_forward_diff(mlab_scalar_fn f, double x, double h, void *ctx)
{
    return (f(x + h, ctx) - f(x, ctx)) / h;
}

double mlab_central_diff(mlab_scalar_fn f, double x, double h, void *ctx)
{
    return (f(x + h, ctx) - f(x - h, ctx)) / (2.0 * h);
}

double mlab_forward3_diff(mlab_scalar_fn f, double x, double h, void *ctx)
{
    /* (-3f(x)+4f(x+h)-f(x+2h))/(2h) */
    return (-3.0 * f(x, ctx) + 4.0 * f(x + h, ctx) - f(x + 2.0 * h, ctx)) / (2.0 * h);
}

double mlab_simpson(mlab_scalar_fn f, double a, double b, int n_panels, void *ctx)
{
    double h, s;
    int i;
    if (n_panels < 2 || (n_panels % 2) != 0) return NAN;
    h = (b - a) / n_panels;
    s = f(a, ctx) + f(b, ctx);
    for (i = 1; i < n_panels; ++i) {
        double x = a + i * h;
        s += ((i % 2) ? 4.0 : 2.0) * f(x, ctx);
    }
    return s * h / 3.0;
}

double mlab_trapezoid(mlab_scalar_fn f, double a, double b, int n_panels, void *ctx)
{
    double h, s;
    int i;
    if (n_panels < 1) return NAN;
    h = (b - a) / n_panels;
    s = 0.5 * (f(a, ctx) + f(b, ctx));
    for (i = 1; i < n_panels; ++i) s += f(a + i * h, ctx);
    return s * h;
}

int mlab_loglog_slope(const double *x, const double *y, int n,
                      double *slope_out, double *intercept_out)
{
    /* fit y = alpha + beta * x  (caller passes log(h), log(err)) */
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    int i;
    double denom, beta, alpha;
    if (n < 2) return -1;
    for (i = 0; i < n; ++i) {
        sx += x[i];
        sy += y[i];
        sxx += x[i] * x[i];
        sxy += x[i] * y[i];
    }
    denom = n * sxx - sx * sx;
    if (fabs(denom) < 1e-300) return -1;
    beta = (n * sxy - sx * sy) / denom;
    alpha = (sy - beta * sx) / n;
    if (slope_out) *slope_out = beta;
    if (intercept_out) *intercept_out = alpha;
    return 0;
}
