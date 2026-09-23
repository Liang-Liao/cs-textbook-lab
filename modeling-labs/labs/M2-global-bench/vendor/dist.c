#include "dist.h"

#include <math.h>

#define MLAB_PI 3.14159265358979323846
#define MLAB_SQRT2 1.41421356237309504880
#define MLAB_SQRT2PI 2.50662827463100050242
#define MLAB_2PI 6.283185307179586476

double mlab_uniform_pdf(double x, double a, double b)
{
    if (x < a || x > b) return 0.0;
    return 1.0 / (b - a);
}

double mlab_uniform_cdf(double x, double a, double b)
{
    if (x <= a) return 0.0;
    if (x >= b) return 1.0;
    return (x - a) / (b - a);
}

/* erf via series + asymptotic (double) */
double mlab_erf(double x)
{
    double ax = fabs(x);
    double t, y;
    if (ax < 2.5) {
        /* Taylor-like: erf(x) = 2/sqrt(pi) * sum (-1)^n x^(2n+1)/(n!(2n+1)) */
        double sum = x, term = x, x2 = x * x;
        int n;
        for (n = 1; n < 80; ++n) {
            term *= -x2 / n;
            sum += term / (2 * n + 1);
            if (fabs(term) < 1e-18 * fabs(sum)) break;
        }
        return (2.0 / sqrt(MLAB_PI)) * sum;
    }
    /* complementary for large |x|: erfc asymptotic */
    t = 1.0 / (1.0 + 0.5 * ax);
    y = t * exp(-ax * ax - 1.26551223 + t * (1.00002368 + t * (0.37409196 +
            t * (0.09678418 + t * (-0.18628806 + t * (0.27886807 +
            t * (-1.13520398 + t * (1.48851587 + t * (-0.82215223 +
            t * 0.17087277)))))))));
    if (x >= 0.0) return 1.0 - y;
    return y - 1.0;
}

double mlab_erfc(double x)
{
    return 1.0 - mlab_erf(x);
}

double mlab_normal_pdf(double x, double mu, double sigma)
{
    double z = (x - mu) / sigma;
    return exp(-0.5 * z * z) / (sigma * MLAB_SQRT2PI);
}

double mlab_normal_cdf(double x, double mu, double sigma)
{
    double z = (x - mu) / (sigma * MLAB_SQRT2);
    return 0.5 * (1.0 + mlab_erf(z));
}

/* Acklam's inverse normal CDF approximation + one Halley step */
double mlab_normal_quantile(double p, double mu, double sigma)
{
    const double a[] = {
        -3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
        1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    const double b[] = {
        -5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
        6.680131188771972e+01, -1.328068155288572e+01};
    const double c[] = {
        -7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
        -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    const double d[] = {
        7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
        3.754408661907416e+00};
    const double plow = 0.02425;
    const double phigh = 1.0 - plow;
    double q, r, x, e, u;

    if (p <= 0.0) return mu - 10.0 * sigma;
    if (p >= 1.0) return mu + 10.0 * sigma;
    if (p < plow) {
        q = sqrt(-2.0 * log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else if (p > phigh) {
        q = sqrt(-2.0 * log(1.0 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0);
    } else {
        q = p - 0.5;
        r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    }
    /* Halley refinement */
    e = 0.5 * mlab_erfc(-x / MLAB_SQRT2) - p;
    u = e * MLAB_SQRT2PI * exp(x * x / 2.0);
    x = x - u / (1.0 + x * u / 2.0);
    return mu + sigma * x;
}

double mlab_expon_pdf(double x, double lambda)
{
    if (x < 0.0) return 0.0;
    return lambda * exp(-lambda * x);
}

double mlab_expon_cdf(double x, double lambda)
{
    if (x < 0.0) return 0.0;
    return 1.0 - exp(-lambda * x);
}

double mlab_expon_quantile(double u, double lambda)
{
    return -log(1.0 - u) / lambda;
}
