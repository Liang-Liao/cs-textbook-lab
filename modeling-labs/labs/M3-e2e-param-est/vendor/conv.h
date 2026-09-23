#ifndef MLAB_CONV_H
#define MLAB_CONV_H

/*
 * Estimate geometric convergence factor from successive residual norms:
 * rho ≈ r_{k+1}/r_k averaged, or log-linear fit on last m steps.
 * Returns estimated rho, or <0 on failure.
 */
double mlab_est_conv_factor(const double *residuals, int n, int last_m);

/* rate p from ||e_k|| ~ C * r_k^p using two successive: p ≈ log(e_{k+1}/e_k)/log(r_{k+1}/r_k) */
double mlab_est_order_pair(double e_km1, double e_k, double e_kp1);

#endif /* MLAB_CONV_H */
