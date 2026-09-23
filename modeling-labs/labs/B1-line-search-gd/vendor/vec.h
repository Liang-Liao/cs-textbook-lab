#ifndef MLAB_VEC_H
#define MLAB_VEC_H

#include <stddef.h>

/* Dense vector of double. data owned by caller unless noted. */
typedef struct {
    int n;
    double *data;
} mlab_vec;

double mlab_dot(const double *a, const double *b, int n);
double mlab_nrm2(const double *x, int n);
double mlab_nrm1(const double *x, int n);
double mlab_nrminf(const double *x, int n);
double mlab_dist2(const double *a, const double *b, int n);

void mlab_vec_zero(double *x, int n);
void mlab_vec_copy(double *dst, const double *src, int n);
void mlab_vec_axpy(double y[], double a, const double *x, int n); /* y += a*x */
void mlab_vec_scale(double x[], double a, int n);

#endif /* MLAB_VEC_H */
