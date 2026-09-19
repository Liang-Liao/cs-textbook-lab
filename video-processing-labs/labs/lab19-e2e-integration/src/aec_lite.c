#include "aec_lite.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* copied from lab08-aec src/aec.c (nlms_step / erle_db), trimmed */

void aec_nlms_init(aec_nlms *f, int L, double mu) {
    f->L = L;
    f->mu = mu;
    f->h = calloc((size_t)L, sizeof(double));
    f->xbuf = calloc((size_t)L, sizeof(double));
}

void aec_nlms_free(aec_nlms *f) {
    free(f->h);
    free(f->xbuf);
    f->h = f->xbuf = NULL;
}

double aec_nlms_step(aec_nlms *f, double x, double d, int freeze) {
    memmove(f->xbuf + 1, f->xbuf, (size_t)(f->L - 1) * sizeof(double));
    f->xbuf[0] = x;
    double y = 0.0;
    for (int i = 0; i < f->L; i++) y += f->h[i] * f->xbuf[i];
    double e = d - y;
    if (!freeze) {
        double px = 1e-8;
        for (int i = 0; i < f->L; i++) px += f->xbuf[i] * f->xbuf[i];
        double a = f->mu * e / px;
        for (int i = 0; i < f->L; i++) f->h[i] += a * f->xbuf[i];
    }
    return e;
}

double aec_erle_db(const double *ref, const double *res, int n) {
    double pr = 0, pe = 0;
    for (int i = 0; i < n; i++) {
        pr += ref[i] * ref[i];
        pe += res[i] * res[i];
    }
    if (pe <= 0) return 200;
    return 10.0 * log10(pr / pe);
}
