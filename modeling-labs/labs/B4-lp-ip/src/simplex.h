#ifndef MLAB_LP_H
#define MLAB_LP_H

/*
 * Shared LP infrastructure (reused by B4 experiments and later D3 modeling).
 * Standard form: min c^T x  s.t. A x = b, x >= 0
 */
int mlab_simplex(const double *A, int m, int n, const double *b, const double *c,
                 double *x_opt, double *obj_out, double *y_out);

#endif /* MLAB_LP_H */
