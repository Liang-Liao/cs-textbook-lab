#ifndef LAB16_MDCT_H
#define LAB16_MDCT_H
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
/* MDCT of length N (hop N/2). x has N samples, X has N/2 coeffs. */
void mdct(const double *x, double *X, int N);
void imdct(const double *X, double *x, int N);
/* Perfect recon: analysis window + MDCT + IMDCT + synthesis window + OLA. */
double mdct_roundtrip_err(const double *sig, int n, int N);
#endif
