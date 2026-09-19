#include "agc.h"
#include <math.h>
#include <stdlib.h>

double compressor_gain_db(const compressor *c, double level_db) {
    double over = level_db - c->thresh_db;
    double gain = 0.0;
    if (c->knee_db > 0.0 && over > -0.5 * c->knee_db && over < 0.5 * c->knee_db) {
        double x = over + 0.5 * c->knee_db;
        gain = (1.0 / c->ratio - 1.0) * (x * x) / (2.0 * c->knee_db);
    } else if (over >= 0.5 * c->knee_db) {
        gain = (1.0 / c->ratio - 1.0) * over;
    }
    return gain + c->makeup_db;
}

double compressor_run_sample(const compressor *c, double x, double *env) {
    /* static gain for the caller-tracked peak envelope */
    double lvl = 20.0 * log10(*env + 1e-12);
    double g = pow(10.0, compressor_gain_db(c, lvl) / 20.0);
    return x * g;
}

void limiter_init(limiter *l, double limit, double fs, double att_ms,
                  double rel_ms, double lookahead_ms) {
    l->limit = limit;
    l->att_coef = 1.0 - exp(-1000.0 / (att_ms * fs + 1e-9));
    l->rel_coef = 1.0 - exp(-1000.0 / (rel_ms * fs + 1e-9));
    l->env_rel_coef = 1.0 - exp(-1000.0 / (20.0 * fs + 1e-9));
    l->gain = 1.0;
    l->env = 0.0;
    l->dlen = (int)(lookahead_ms * fs / 1000.0);
    if (l->dlen < 1) l->dlen = 1;
    l->dl = calloc((size_t)l->dlen, sizeof(double));
    l->dpos = 0;
}

void limiter_free(limiter *l) {
    free(l->dl);
    l->dl = NULL;
}

double limiter_process(limiter *l, double x) {
    double ax = fabs(x);
    /* envelope: instant attack, smooth release */
    l->env = (ax > l->env) ? ax : l->env + l->env_rel_coef * (ax - l->env);
    /* desired gain from the *future* peak: the envelope sees the undelayed
     * input, so the gain is already down when a peak leaves the delay line */
    double gdes = (l->env > l->limit) ? l->limit / l->env : 1.0;
    l->gain += (gdes < l->gain) ? l->att_coef * (gdes - l->gain)
                                : l->rel_coef * (gdes - l->gain);
    double xd = l->dl[l->dpos];
    l->dl[l->dpos] = x;
    l->dpos = (l->dpos + 1) % l->dlen;
    double y = xd * l->gain;
    /* brick-wall safety net — must never engage while lookahead is active */
    if (y > 0.999) y = 0.999;
    if (y < -0.999) y = -0.999;
    return y;
}

void agc_init(agc *a, double target_db, double fs, double att_s, double rel_s) {
    a->target_db = target_db;
    a->att_coef = 1.0 - exp(-1.0 / (att_s * fs + 1e-9));
    a->rel_coef = 1.0 - exp(-1.0 / (rel_s * fs + 1e-9));
    a->max_gain_db = 40.0;
    a->min_gain_db = -20.0;
    a->gain = 1.0;
    a->env = 1e-3;
    a->peak_limit = 0.99;
}

double agc_process(agc *a, double x) {
    double ax = fabs(x);
    double ecoef = (ax > a->env) ? a->att_coef : a->rel_coef;
    a->env += ecoef * (ax - a->env);
    if (a->env < 1e-6) a->env = 1e-6;

    double target_lin = pow(10.0, a->target_db / 20.0);
    double gdes = target_lin / a->env;
    double gmax = pow(10.0, a->max_gain_db / 20.0);
    double gmin = pow(10.0, a->min_gain_db / 20.0);
    if (gdes > gmax) gdes = gmax;
    if (gdes < gmin) gdes = gmin;

    /* fast attack when reducing gain (input grew), slow release when raising gain */
    if (gdes < a->gain)
        a->gain += a->att_coef * (gdes - a->gain);
    else
        a->gain += a->rel_coef * (gdes - a->gain);

    double y = x * a->gain;
    if (y > a->peak_limit) y = a->peak_limit;
    if (y < -a->peak_limit) y = -a->peak_limit;
    return y;
}

double rms_db(const double *x, int n) {
    double s = 0;
    for (int i = 0; i < n; i++) s += x[i] * x[i];
    s /= n;
    return 10.0 * log10(s + 1e-30);
}

double peak_abs(const double *x, int n) {
    double p = 0;
    for (int i = 0; i < n; i++) {
        double a = fabs(x[i]);
        if (a > p) p = a;
    }
    return p;
}
