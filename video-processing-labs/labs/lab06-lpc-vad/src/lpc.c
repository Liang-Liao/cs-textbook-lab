#include "lpc.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void autocorr(const double *x, int n, int p, double *r) {
    if (!x || !r || n <= 0 || p < 0) return;
    for (int k = 0; k <= p; k++) {
        double acc = 0.0;
        for (int i = 0; i < n - k; i++) acc += x[i] * x[i + k];
        r[k] = acc;
    }
}

int levinson_durbin(const double *r, int p, double *a, double *err) {
    if (!r || !a || p < 1) return -1;
    double e = r[0];
    if (e <= 0.0) return -1;
    a[0] = 1.0;
    for (int i = 1; i <= p; i++) a[i] = 0.0;
    double *tmp = (double *)malloc((size_t)(p + 1) * sizeof(double));
    if (!tmp) return -1;
    for (int i = 1; i <= p; i++) {
        double acc = r[i];
        for (int j = 1; j < i; j++) acc += a[j] * r[i - j];
        double k = -acc / e;
        tmp[i] = k;
        for (int j = 1; j < i; j++) tmp[j] = a[j] + k * a[i - j];
        for (int j = 1; j <= i; j++) a[j] = tmp[j];
        e *= (1.0 - k * k);
        if (e <= 0.0) {
            free(tmp);
            return -1;
        }
    }
    free(tmp);
    if (err) *err = e;
    return 0;
}

void lpc_envelope(const double *a, int p, int nfft, double *env_db) {
    if (!a || !env_db || p < 1 || nfft < 2) return;
    for (int k = 0; k < nfft; k++) {
        double w = 2.0 * M_PI * (double)k / (double)nfft;
        double re = 0.0, im = 0.0;
        for (int i = 0; i <= p; i++) {
            double ang = -w * (double)i;
            re += a[i] * cos(ang);
            im += a[i] * sin(ang);
        }
        double mag = sqrt(re * re + im * im);
        if (mag < 1e-20) mag = 1e-20;
        env_db[k] = -20.0 * log10(mag);
    }
}

int formants_from_envelope(const double *env_db, int nfft, double fs, int max_formants,
                           double *f_hz) {
    if (!env_db || !f_hz || nfft < 8) return 0;
    int nyq = nfft / 2;
    int count = 0;
    /* ignore very low bins (F0 region) and near Nyquist */
    int kmin = (int)lrint(150.0 * (double)nfft / fs);
    int kmax = (int)lrint((fs * 0.5 - 200.0) * (double)nfft / fs);
    if (kmin < 2) kmin = 2;
    if (kmax > nyq - 2) kmax = nyq - 2;
    double gmax = -1e300;
    for (int k = kmin; k <= kmax; k++) {
        if (env_db[k] > gmax) gmax = env_db[k];
    }
    for (int k = kmin; k <= kmax && count < max_formants; k++) {
        if (env_db[k] > env_db[k - 1] && env_db[k] >= env_db[k + 1]) {
            /* drop peaks more than 40 dB below the strongest peak */
            if (env_db[k] < gmax - 40.0) continue;
            /* local valley prominence */
            double left = env_db[k], right = env_db[k];
            for (int j = k - 1; j >= kmin; j--) {
                if (env_db[j] < left) left = env_db[j];
                else if (env_db[j] > env_db[k] - 1e-9) break;
            }
            for (int j = k + 1; j <= kmax; j++) {
                if (env_db[j] < right) right = env_db[j];
                else if (env_db[j] > env_db[k] - 1e-9) break;
            }
            double prom = env_db[k] - ((left > right) ? left : right);
            if (prom < 2.0) continue;
            /* parabolic interpolation for sub-bin Hz */
            double y0 = env_db[k - 1], y1 = env_db[k], y2 = env_db[k + 1];
            double denom = (y0 - 2.0 * y1 + y2);
            double delta = 0.0;
            if (fabs(denom) > 1e-12) delta = 0.5 * (y0 - y2) / denom;
            if (delta < -0.5) delta = -0.5;
            if (delta > 0.5) delta = 0.5;
            double fk = ((double)k + delta) * fs / (double)nfft;
            f_hz[count++] = fk;
        }
    }
    return count;
}

/* Multiply two polynomials (ascending coeffs). */
static void polymul(const double *a, int na, const double *b, int nb, double *out) {
    for (int i = 0; i < na + nb - 1; i++) out[i] = 0.0;
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++) out[i + j] += a[i] * b[j];
}

void lpc_from_formants(const double *f, const double *bw, int n_f, double fs, double *a,
                       int p) {
    if (!f || !bw || !a || n_f <= 0 || p < 2 * n_f) {
        if (a && p >= 0) {
            a[0] = 1.0;
            for (int i = 1; i <= p; i++) a[i] = 0.0;
        }
        return;
    }
    /* Start with A(z)=1; for each formant convolve with 2nd-order resonator
     * A_i(z) = 1 - 2 r cos(theta) z^{-1} + r^2 z^{-2}, r=exp(-pi B/fs). */
    double *acc = (double *)malloc((size_t)(p + 1) * sizeof(double));
    double *tmp = (double *)malloc((size_t)(p + 1) * sizeof(double));
    if (!acc || !tmp) {
        free(acc);
        free(tmp);
        return;
    }
    for (int i = 0; i <= p; i++) acc[i] = 0.0;
    acc[0] = 1.0;
    int cur_deg = 0;
    for (int m = 0; m < n_f; m++) {
        double r = exp(-M_PI * bw[m] / fs);
        double th = 2.0 * M_PI * f[m] / fs;
        double sec[3];
        sec[0] = 1.0;
        sec[1] = -2.0 * r * cos(th);
        sec[2] = r * r;
        polymul(acc, cur_deg + 1, sec, 3, tmp);
        cur_deg += 2;
        for (int i = 0; i <= cur_deg && i <= p; i++) acc[i] = tmp[i];
    }
    for (int i = 0; i <= p; i++) a[i] = (i <= cur_deg) ? acc[i] : 0.0;
    free(acc);
    free(tmp);
}

void lpc_synthesize_vowel(const double *a, int p, int n, double fs, double f0, double *x) {
    if (!a || !x || n <= 0) return;
    int period = (int)lrint(fs / f0);
    if (period < 2) period = 2;
    double *y = (double *)calloc((size_t)n, sizeof(double));
    if (!y) return;
    /* residual: impulse train with slight jitter-free amplitude */
    for (int i = 0; i < n; i += period) y[i] = 1.0;
    /* filter 1/A(z): y[n] = x[n] - sum a_k y[n-k]  (excitation x stored in y as in-place
     * is messy — use separate buffer) */
    double *exc = (double *)malloc((size_t)n * sizeof(double));
    if (!exc) {
        free(y);
        return;
    }
    memcpy(exc, y, (size_t)n * sizeof(double));
    memset(y, 0, (size_t)n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double s = exc[i];
        for (int k = 1; k <= p; k++) {
            int j = i - k;
            if (j >= 0) s -= a[k] * y[j];
        }
        y[i] = s;
    }
    /* normalize peak */
    double peak = 0.0;
    for (int i = 0; i < n; i++) {
        double v = fabs(y[i]);
        if (v > peak) peak = v;
    }
    if (peak > 1e-12) {
        for (int i = 0; i < n; i++) y[i] *= 0.8 / peak;
    }
    memcpy(x, y, (size_t)n * sizeof(double));
    free(y);
    free(exc);
}
