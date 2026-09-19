/* simplified AGC from lab09 */
#include "agc.h"
#include <math.h>

void agc_init(agc *a, double target_db, double fs, double att_s, double rel_s) {
    a->target_db = target_db;
    a->att_coef = 1.0 - exp(-1.0 / (att_s * fs + 1e-9));
    a->rel_coef = 1.0 - exp(-1.0 / (rel_s * fs + 1e-9));
    a->gain = 1.0;
    a->env = 1e-3;
    a->peak_limit = 0.99;
    a->injected_scale = 1.0;
}

double agc_process(agc *a, double x) {
    double ax = fabs(x);
    double ecoef = (ax > a->env) ? a->att_coef : a->rel_coef;
    a->env += ecoef * (ax - a->env);
    if (a->env < 1e-6) a->env = 1e-6;
    double target_lin = pow(10.0, a->target_db / 20.0);
    double gdes = target_lin / a->env;
    if (gdes > 8.0) gdes = 8.0;
    if (gdes < 0.125) gdes = 0.125;
    if (gdes < a->gain)
        a->gain += a->att_coef * (gdes - a->gain);
    else
        a->gain += a->rel_coef * (gdes - a->gain);
    double y = x * a->gain * a->injected_scale;
    if (y > a->peak_limit) y = a->peak_limit;
    if (y < -a->peak_limit) y = -a->peak_limit;
    return y;
}
