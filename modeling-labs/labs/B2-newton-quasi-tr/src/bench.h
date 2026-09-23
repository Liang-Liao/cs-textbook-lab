#ifndef MLAB_BENCH_H
#define MLAB_BENCH_H

/* Standard benchmark objectives used across B/C/M layers. dim n>=2 unless noted. */
double mlab_sphere(const double *x, int n, void *ctx);
void mlab_sphere_grad(const double *x, int n, double *g, void *ctx);
void mlab_sphere_hess(const double *x, int n, double *H, void *ctx);

double mlab_rosenbrock(const double *x, int n, void *ctx);
void mlab_rosenbrock_grad(const double *x, int n, double *g, void *ctx);
void mlab_rosenbrock_hess(const double *x, int n, double *H, void *ctx);

/* quadratic 0.5 x^T A x with A provided via ctx (row-major n*n, SPD) */
typedef struct {
    const double *A;
    int n;
} mlab_quad_ctx;
double mlab_quad(const double *x, int n, void *ctx);
void mlab_quad_grad(const double *x, int n, double *g, void *ctx);
void mlab_quad_hess(const double *x, int n, double *H, void *ctx);

#endif /* MLAB_BENCH_H */
