#include "conv.h"

#include <math.h>

double mlab_est_conv_factor(const double *residuals, int n, int last_m)
{
    int start, i;
    double sum = 0.0;
    int cnt = 0;
    if (n < 2 || last_m < 1) return -1.0;
    start = n - last_m;
    if (start < 1) start = 1;
    for (i = start; i < n; ++i) {
        if (residuals[i - 1] <= 0.0) continue;
        sum += residuals[i] / residuals[i - 1];
        ++cnt;
    }
    if (cnt == 0) return -1.0;
    return sum / cnt;
}

double mlab_est_order_pair(double e_km1, double e_k, double e_kp1)
{
    double r1, r2, p1;
    if (e_km1 <= 0 || e_k <= 0 || e_kp1 <= 0) return -1.0;
    r1 = e_k / e_km1;
    r2 = e_kp1 / e_k;
    if (r1 <= 0 || r2 <= 0 || fabs(r1 - 1.0) < 1e-16) return -1.0;
    p1 = log(r2) / log(r1);
    return p1;
}
