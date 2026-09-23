#ifndef MLAB_OPT_H
#define MLAB_OPT_H

/* Generic unconstrained objective */
typedef struct {
    int dim;
    double (*f)(const double *x, int n, void *ctx);
    void (*grad)(const double *x, int n, double *g, void *ctx);
    void (*hess)(const double *x, int n, double *H, void *ctx); /* optional row-major */
    void *ctx;
} mlab_objective;

typedef struct {
    double x_final_norm;   /* ||x - x*|| if known, else -1 */
    double f_final;
    double grad_norm;
    int iters;
    int fevals;
    int status; /* 0 ok, 1 max_iter, 2 other */
} mlab_opt_result;

#endif /* MLAB_OPT_H */
