#ifndef LAB03_CONV_H
#define LAB03_CONV_H

/* Linear convolution y[n] = sum_k h[k] x[n-k], length n+m-1. */
void conv_direct(const double *x, int n, const double *h, int m, double *y);

/* FFT-based linear convolution (zero-pad to next pow2 >= n+m-1). */
void conv_fft(const double *x, int n, const double *h, int m, double *y);

double rel_max_err_vec(const double *a, const double *b, int n);

#endif
