#ifndef LAB12_CONV2D_H
#define LAB12_CONV2D_H

int conv2d_direct(const double *src, int w, int h, const double *k, int kw, int kh,
                  double *dst);
int conv2d_separable(const double *src, int w, int h, const double *kx, int klen,
                     const double *ky, double *tmp, double *dst);
void gauss_kernel_1d(double *k, int n, double sigma);
void gauss_kernel_2d(double *k, int n, double sigma);
double rel_max_err(const double *a, const double *b, int n);

#endif
