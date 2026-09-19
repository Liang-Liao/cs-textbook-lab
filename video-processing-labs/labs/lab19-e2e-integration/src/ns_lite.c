#include "ns_lite.h"
#include <math.h>

void ns_lite_init(ns_lite *n, double floor_gain) {
    n->floor_gain = floor_gain;
    n->noise_ema = 1e-4;
    n->alpha = 0.01;
}

double ns_lite_process(ns_lite *n, double x) {
    double ax = fabs(x);
    /* track noise floor slowly upward, faster downward */
    if (ax < n->noise_ema)
        n->noise_ema += 0.05 * (ax - n->noise_ema);
    else
        n->noise_ema += n->alpha * (ax - n->noise_ema);
    double ref = n->noise_ema * 4.0 + 1e-6;
    double g = 1.0;
    if (ax < ref) {
        g = n->floor_gain + (1.0 - n->floor_gain) * (ax / ref);
    }
    return x * g;
}
