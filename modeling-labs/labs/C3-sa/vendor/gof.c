#include "gof.h"
#include "stats.h"

#include <math.h>
#include <stdlib.h>

double mlab_ks_statistic(const double *xs_sorted, int n, mlab_cdf_fn F, void *ctx)
{
    double dmax = 0.0;
    int i;
    if (n <= 0 || !F) return 0.0;
    for (i = 0; i < n; ++i) {
        double Fn_lo = (double)i / (double)n;           /* just before x_(i+1) */
        double Fn_hi = (double)(i + 1) / (double)n;     /* at x_(i+1) */
        double Fx = F(xs_sorted[i], ctx);
        double d1 = fabs(Fn_hi - Fx);
        double d2 = fabs(Fx - Fn_lo);
        if (d1 > dmax) dmax = d1;
        if (d2 > dmax) dmax = d2;
    }
    return dmax;
}

double mlab_ks_crit_alpha05(int n)
{
    if (n <= 0) return 1.0;
    return 1.358101515751575 / sqrt((double)n); /* ~ c(0.05)/sqrt(n) */
}

/* lower incomplete gamma P(a,x) series / continued fraction; then SF = 1-P for chi2 */
static double gammq(double a, double x)
{
    /* Q(a,x) upper incomplete / Gamma(a) */
    const int ITMAX = 200;
    const double EPS = 3e-14;
    double gln = lgamma(a);
    int n;
    if (x < 0 || a <= 0) return 1.0;
    if (x < a + 1.0) {
        /* series for P */
        double ap = a, sum = 1.0 / a, del = sum;
        for (n = 1; n <= ITMAX; ++n) {
            ++ap;
            del *= x / ap;
            sum += del;
            if (fabs(del) < fabs(sum) * EPS) break;
        }
        return 1.0 - sum * exp(-x + a * log(x) - gln);
    }
    /* continued fraction for Q */
    {
        double b = x + 1.0 - a;
        double c = 1.0 / 1e-300;
        double d = 1.0 / b;
        double h = d;
        for (n = 1; n <= ITMAX; ++n) {
            double an = -n * (n - a);
            b += 2.0;
            d = an * d + b;
            if (fabs(d) < 1e-300) d = 1e-300;
            c = b + an / c;
            if (fabs(c) < 1e-300) c = 1e-300;
            d = 1.0 / d;
            {
                double del = d * c;
                h *= del;
                if (fabs(del - 1.0) < EPS) break;
            }
        }
        return exp(-x + a * log(x) - gln) * h;
    }
}

double mlab_chi2_gof(const long *obs, const double *exp, int k)
{
    double s = 0.0;
    int i;
    for (i = 0; i < k; ++i) {
        if (exp[i] <= 0.0) continue;
        {
            double d = (double)obs[i] - exp[i];
            s += d * d / exp[i];
        }
    }
    return s;
}

double mlab_chi2_sf(double chi2, int df)
{
    if (df <= 0) return 1.0;
    if (chi2 <= 0.0) return 1.0;
    return gammq(0.5 * df, 0.5 * chi2);
}
