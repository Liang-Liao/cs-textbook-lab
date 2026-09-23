#ifndef MLAB_DIST_H
#define MLAB_DIST_H

/* PDF / CDF helpers (no quantile library beyond what labs need) */

double mlab_uniform_pdf(double x, double a, double b);
double mlab_uniform_cdf(double x, double a, double b);

double mlab_normal_pdf(double x, double mu, double sigma);
double mlab_normal_cdf(double x, double mu, double sigma);
/* inverse CDF via Acklam / rational approximation + Newton polish */
double mlab_normal_quantile(double p, double mu, double sigma);

double mlab_expon_pdf(double x, double lambda);
double mlab_expon_cdf(double x, double lambda);
double mlab_expon_quantile(double u, double lambda);

/* erf via complementary error function (Abramowitz-Stegun / Cephes-style) */
double mlab_erf(double x);
double mlab_erfc(double x);

#endif /* MLAB_DIST_H */
