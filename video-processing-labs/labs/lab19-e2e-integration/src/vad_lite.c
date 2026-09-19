#include "vad_lite.h"
#include <math.h>

/* copied from lab06 vad (noise-floor tracking idea), frame-level trim */

void vad_lite_init(vad_lite *v, double thr_db, int hangover) {
    v->floor_pow = 1e-4;
    v->thr_db = thr_db;
    v->hangover = hangover;
    v->hold = 0;
}

int vad_lite_frame(vad_lite *v, const double *x, int n) {
    double e = 0;
    for (int i = 0; i < n; i++) e += x[i] * x[i];
    e /= n;
    /* asymmetric floor tracking: fast down, slow up (speech must not raise it) */
    if (e < v->floor_pow)
        v->floor_pow += 0.5 * (e - v->floor_pow);
    else
        v->floor_pow += 0.001 * (e - v->floor_pow);
    if (v->floor_pow < 1e-8) v->floor_pow = 1e-8;
    double ratio_db = 10.0 * log10(e / v->floor_pow);
    int speech = ratio_db > v->thr_db;
    if (speech)
        v->hold = v->hangover;
    else if (v->hold > 0)
        v->hold--;
    return (speech || v->hold > 0) ? 1 : 0;
}
