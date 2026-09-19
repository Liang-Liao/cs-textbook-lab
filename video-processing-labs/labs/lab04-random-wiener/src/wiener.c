#include "wiener.h"
#include "fft.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void wiener_gain_from_psd(const double *Ps, const double *Pn, int n, double *H) {
    for (int i = 0; i < n; i++) {
        double den = Ps[i] + Pn[i];
        H[i] = (den > 1e-30) ? (Ps[i] / den) : 0.0;
    }
}

void apply_wiener_frame(const double *frame, const double *Ps, const double *Pn,
                        int n, double *out) {
    double *H = malloc((size_t)n * sizeof(double));
    cpx *X = calloc((size_t)n, sizeof(cpx));
    if (!H || !X) { free(H); free(X); return; }
    wiener_gain_from_psd(Ps, Pn, n, H);
    for (int i = 0; i < n; i++) { X[i].re = frame[i]; X[i].im = 0.0; }
    fft(X, n);
    for (int i = 0; i < n; i++) { X[i].re *= H[i]; X[i].im *= H[i]; }
    ifft(X, n);
    for (int i = 0; i < n; i++) out[i] = X[i].re;
    free(H); free(X);
}

double snr_db(const double *ref, const double *test, size_t n) {
    double ps = 0.0, pe = 0.0;
    for (size_t i = 0; i < n; i++) {
        double e = test[i] - ref[i];
        ps += ref[i] * ref[i];
        pe += e * e;
    }
    if (pe <= 0.0) return 200.0;
    return 10.0 * log10(ps / pe);
}
