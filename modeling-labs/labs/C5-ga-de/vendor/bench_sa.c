#include "bench_sa.h"

#include <math.h>

#ifndef MLAB_PI
#define MLAB_PI 3.14159265358979323846
#endif

double mlab_rastrigin(const double *x, int n, void *ctx)
{
    double s = 10.0 * n;
    int i;
    (void)ctx;
    if (!x || n <= 0) return 0.0;
    for (i = 0; i < n; ++i)
        s += x[i] * x[i] - 10.0 * cos(2.0 * MLAB_PI * x[i]);
    return s;
}

void mlab_rastrigin_bounds(int n, double *lb, double *ub)
{
    int i;
    for (i = 0; i < n; ++i) {
        lb[i] = -5.12;
        ub[i] = 5.12;
    }
}
