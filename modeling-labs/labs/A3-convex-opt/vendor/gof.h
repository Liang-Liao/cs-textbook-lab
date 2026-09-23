#ifndef MLAB_TEST_H
#define MLAB_TEST_H

/*
 * Kolmogorov-Smirnov one-sample test.
 * xs_sorted: ascending sample; F: theoretical CDF.
 * Returns D_n = sup |F_n - F|. Critical value for level alpha:
 *   D_crit ≈ c(alpha)/sqrt(n), c(0.10)=1.22, c(0.05)=1.36, c(0.01)=1.63
 * Reject H0 if D_n > D_crit.
 */
typedef double (*mlab_cdf_fn)(double x, void *ctx);

double mlab_ks_statistic(const double *xs_sorted, int n, mlab_cdf_fn F, void *ctx);
double mlab_ks_crit_alpha05(int n);

/*
 * Chi-square GOF: observed counts in k bins vs expected counts.
 * Returns chi2 statistic. df = k-1-0 (no estimated params) unless df_adj used.
 * p-value via complementary incomplete gamma (upper tail).
 */
double mlab_chi2_gof(const long *obs, const double *exp, int k);
double mlab_chi2_sf(double chi2, int df); /* P(X > chi2) */

#endif /* MLAB_TEST_H */
