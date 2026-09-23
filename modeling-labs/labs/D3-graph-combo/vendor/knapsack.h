#ifndef KNAPSACK_H
#define KNAPSACK_H

/* Lab-local to B4: vertex enumeration + 0-1 knapsack BnB vs brute. */

int mlab_vertex_enum(const double *A, int m, int n, const double *b, const double *c,
                     double *x_opt, double *obj_out);
double mlab_knapsack_bb(const double *w, const double *v, int n, double cap, int *x_sel);
double mlab_knapsack_brute(const double *w, const double *v, int n, double cap, int *x_sel);

#endif
