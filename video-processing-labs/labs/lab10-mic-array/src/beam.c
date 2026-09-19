#include "beam.h"
#include <math.h>
#include <stdlib.h>

void fft(cpx *data, int n) {
    if (!data || n <= 1) return;
    int half = n / 2;
    cpx *e = malloc((size_t)half * sizeof(cpx));
    cpx *o = malloc((size_t)half * sizeof(cpx));
    if (!e || !o) { free(e); free(o); return; }
    for (int i = 0; i < half; i++) { e[i]=data[2*i]; o[i]=data[2*i+1]; }
    fft(e, half); fft(o, half);
    for (int k = 0; k < half; k++) {
        double a = -2.0 * M_PI * k / n;
        double wr=cos(a), wi=sin(a);
        double tr=wr*o[k].re-wi*o[k].im, ti=wr*o[k].im+wi*o[k].re;
        data[k].re=e[k].re+tr; data[k].im=e[k].im+ti;
        data[k+half].re=e[k].re-tr; data[k+half].im=e[k].im-ti;
    }
    free(e); free(o);
}
void ifft(cpx *d, int n) {
    for (int i=0;i<n;i++) d[i].im=-d[i].im;
    fft(d,n);
    for (int i=0;i<n;i++){ d[i].re/=n; d[i].im=-d[i].im/n; }
}

double xcorr_lag(const double *x, const double *y, int n, int max_lag) {
    int best = 0;
    double bestv = -1e300;
    for (int lag = -max_lag; lag <= max_lag; lag++) {
        double s = 0.0;
        int cnt = 0;
        for (int i = 0; i < n; i++) {
            int j = i - lag;
            if (j >= 0 && j < n) { s += x[i] * y[j]; cnt++; }
        }
        if (cnt) s /= cnt;
        if (s > bestv) { bestv = s; best = lag; }
    }
    /* parabolic */
    double ym = 0, y0 = bestv, yp = 0;
    for (int i = 0; i < n; i++) {
        int j = i - (best - 1);
        if (j >= 0 && j < n) ym += x[i] * y[j];
        j = i - (best + 1);
        if (j >= 0 && j < n) yp += x[i] * y[j];
    }
    ym /= n; yp /= n;
    double den = ym - 2 * y0 + yp;
    double frac = 0;
    if (fabs(den) > 1e-30) frac = 0.5 * (ym - yp) / den;
    return (double)best + frac;
}

double gcc_phat(const double *x, const double *y, int n, int max_lag) {
    int N = 1; while (N < n * 2) N <<= 1;
    cpx *X = calloc((size_t)N, sizeof(cpx));
    cpx *Y = calloc((size_t)N, sizeof(cpx));
    if (!X || !Y) { free(X); free(Y); return 0; }
    for (int i = 0; i < n; i++) { X[i].re=x[i]; Y[i].re=y[i]; }
    fft(X, N); fft(Y, N);
    for (int i = 0; i < N; i++) {
        double re = X[i].re*Y[i].re + X[i].im*Y[i].im;
        double im = X[i].im*Y[i].re - X[i].re*Y[i].im;
        double mag = sqrt(re*re + im*im) + 1e-30;
        X[i].re = re/mag; X[i].im = im/mag;
    }
    ifft(X, N);
    int best = 0;
    double bestv = -1e300;
    for (int lag = -max_lag; lag <= max_lag; lag++) {
        int idx = (lag + N) % N;
        if (X[idx].re > bestv) { bestv = X[idx].re; best = lag; }
    }
    int i0 = (best - 1 + N) % N, i1 = (best + N) % N, i2 = (best + 1 + N) % N;
    double y0 = X[i0].re, y1 = X[i1].re, y2 = X[i2].re;
    double denom = (y0 - 2*y1 + y2);
    double frac = 0.0;
    if (fabs(denom) > 1e-30) frac = 0.5 * (y0 - y2) / denom;
    free(X); free(Y);
    return (double)best + frac;
}

double beam_pattern_gain(const double *mic_re, const double *mic_im,
                         int nch, double spacing_m, double fs, double c,
                         double f, double look_deg, double src_deg) {
    (void)fs;
    /* Plane-wave steering: phase at mic m for src angle: m * d * sin(theta) * 2pi f / c */
    double k = 2.0 * M_PI * f / c;
    double re = 0, im = 0;
    for (int m = 0; m < nch; m++) {
        double tau_src = (double)m * spacing_m * sin(src_deg * M_PI / 180.0) / c;
        double tau_look = (double)m * spacing_m * sin(look_deg * M_PI / 180.0) / c;
        double dphi = k * (tau_src - tau_look) * c; /* simplify: k*d*(sin_s - sin_l)*m */
        dphi = k * spacing_m * (double)m * (sin(src_deg*M_PI/180.0) - sin(look_deg*M_PI/180.0));
        double cs = cos(dphi), sn = sin(dphi);
        /* mic signal assumed already as complex spectrum component; here use unit amp with phase */
        double sr = mic_re[m], si = mic_im[m];
        /* apply look steering */
        re += sr * cs - si * sn; /* rotate by -dphi approx */
        im += sr * sn + si * cs;
    }
    re /= nch; im /= nch;
    return sqrt(re*re + im*im);
}
