#ifndef MLAB_NUMCAL_H
#define MLAB_NUMCAL_H

/* f: R -> R */
typedef double (*mlab_scalar_fn)(double x, void *ctx);
/* f: R^n -> R */
typedef double (*mlab_multivar_fn)(const double *x, int n, void *ctx);

/* Finite differences at x */
double mlab_forward_diff(mlab_scalar_fn f, double x, double h, void *ctx);
double mlab_central_diff(mlab_scalar_fn f, double x, double h, void *ctx);
/* Second-order one-sided (3-point forward) */
double mlab_forward3_diff(mlab_scalar_fn f, double x, double h, void *ctx);

/*
 * Composite Simpson on [a,b] with n_panels (must be even, >=2).
 * Returns integral estimate; on bad n returns NaN.
 */
double mlab_simpson(mlab_scalar_fn f, double a, double b, int n_panels, void *ctx);
/* Composite trapezoid, n_panels >= 1 */
double mlab_trapezoid(mlab_scalar_fn f, double a, double b, int n_panels, void *ctx);

/*
 * Least-squares slope of y vs x (linear fit): y ≈ alpha + beta * x.
 * Writes beta (slope) and optionally alpha. Returns 0 on success.
 */
int mlab_loglog_slope(const double *x, const double *y, int n,
                      double *slope_out, double *intercept_out);

#endif /* MLAB_NUMCAL_H */
