#include "filters.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double sinc(double x) {
    if (fabs(x) < 1e-12) return 1.0;
    return sin(M_PI * x) / (M_PI * x);
}

void fir_design_lowpass(double *h, int n_taps, double fs, double fc) {
    if (n_taps < 1) return;
    double nyq = fs * 0.5;
    double fcn = fc / nyq; /* normalized 0..1 of Nyquist */
    double mid = (n_taps - 1) / 2.0; /* fractional center keeps type-I
                                      * symmetry exact for EVEN tap counts */
    double sum = 0.0;
    for (int i = 0; i < n_taps; i++) {
        double k = (double)i - mid;
        double w = 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)(n_taps - 1)); /* Hann */
        double t = (fabs(k) < 1e-12) ? fcn : fcn * sinc(k * fcn);
        h[i] = t * w;
        sum += h[i];
    }
    if (sum != 0.0) {
        for (int i = 0; i < n_taps; i++) h[i] /= sum;
    }
}

void fir_apply(const double *h, int n_taps, const double *x, int n, double *y) {
    for (int i = 0; i < n; i++) {
        double acc = 0.0;
        for (int k = 0; k < n_taps; k++) {
            int j = i - k;
            if (j >= 0) acc += h[k] * x[j];
        }
        y[i] = acc;
    }
}

void biquad_lowpass(biquad *b, double fs, double f0, double q) {
    double w0 = 2.0 * M_PI * f0 / fs;
    double cosw = cos(w0), sinw = sin(w0);
    double alpha = sinw / (2.0 * q);
    double b0 = (1.0 - cosw) / 2.0, b1 = 1.0 - cosw, b2 = (1.0 - cosw) / 2.0;
    double a0 = 1.0 + alpha, a1 = -2.0 * cosw, a2 = 1.0 - alpha;
    b->b0 = b0 / a0;
    b->b1 = b1 / a0;
    b->b2 = b2 / a0;
    b->a1 = a1 / a0;
    b->a2 = a2 / a0;
}

void biquad_peaking(biquad *b, double fs, double f0, double q, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * f0 / fs;
    double cosw = cos(w0), sinw = sin(w0);
    double alpha = sinw / (2.0 * q);
    double b0 = 1.0 + alpha * A, b1 = -2.0 * cosw, b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A, a1 = -2.0 * cosw, a2 = 1.0 - alpha / A;
    b->b0 = b0 / a0;
    b->b1 = b1 / a0;
    b->b2 = b2 / a0;
    b->a1 = a1 / a0;
    b->a2 = a2 / a0;
}

void biquad_highshelf(biquad *b, double fs, double f0, double q, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * f0 / fs;
    double cosw = cos(w0), sinw = sin(w0);
    double alpha = sinw / (2.0 * q);
    double beta = 2.0 * sqrt(A) * alpha;
    double b0 = A * ((A + 1.0) + (A - 1.0) * cosw + beta);
    double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw);
    double b2 = A * ((A + 1.0) + (A - 1.0) * cosw - beta);
    double a0 = (A + 1.0) - (A - 1.0) * cosw + beta;
    double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosw);
    double a2 = (A + 1.0) - (A - 1.0) * cosw - beta;
    b->b0 = b0 / a0;
    b->b1 = b1 / a0;
    b->b2 = b2 / a0;
    b->a1 = a1 / a0;
    b->a2 = a2 / a0;
}

void biquad_reset(biquad_state *s, biquad c) {
    s->c = c;
    s->x1 = s->x2 = s->y1 = s->y2 = 0.0;
}

double biquad_step(biquad_state *s, double x) {
    double y = s->c.b0 * x + s->c.b1 * s->x1 + s->c.b2 * s->x2 - s->c.a1 * s->y1 -
               s->c.a2 * s->y2;
    s->x2 = s->x1;
    s->x1 = x;
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

void biquad_process(const biquad *c, const double *x, int n, double *y) {
    biquad_state s;
    biquad_reset(&s, *c);
    for (int i = 0; i < n; i++) y[i] = biquad_step(&s, x[i]);
}

double biquad_mag(const biquad *b, double fs, double f) {
    double w = 2.0 * M_PI * f / fs;
    double c1 = cos(w), s1 = sin(w);
    double c2 = cos(2 * w), s2 = sin(2 * w);
    /* H = (b0 + b1 e^{-jw} + b2 e^{-j2w}) / (1 + a1 e^{-jw} + a2 e^{-j2w}) */
    double nr = b->b0 + b->b1 * c1 + b->b2 * c2;
    double ni = -(b->b1 * s1 + b->b2 * s2);
    double dr = 1.0 + b->a1 * c1 + b->a2 * c2;
    double di = -(b->a1 * s1 + b->a2 * s2);
    double num = sqrt(nr * nr + ni * ni);
    double den = sqrt(dr * dr + di * di);
    if (den < 1e-300) return 0.0;
    return num / den;
}

double fir_mag(const double *h, int n_taps, double fs, double f) {
    double w = 2.0 * M_PI * f / fs;
    double re = 0.0, im = 0.0;
    for (int i = 0; i < n_taps; i++) {
        re += h[i] * cos(w * i);
        im -= h[i] * sin(w * i);
    }
    return sqrt(re * re + im * im);
}

double biquad_find_cutoff(const biquad *b, double fs, double f0_guess) {
    double mag0 = biquad_mag(b, fs, f0_guess);
    /* binary search frequency where |H|=|H(f0)|/sqrt(2) relative to DC gain.
       For lowpass, |H(0)|≈1, -3dB is 1/sqrt(2). */
    double target = 1.0 / sqrt(2.0);
    double lo = 1.0, hi = fs * 0.49;
    for (int i = 0; i < 60; i++) {
        double mid = 0.5 * (lo + hi);
        if (biquad_mag(b, fs, mid) > target)
            lo = mid;
        else
            hi = mid;
    }
    (void)mag0;
    return 0.5 * (lo + hi);
}

double fir_impulse_asymmetry(const double *h, int n_taps) {
    double worst = 0.0;
    for (int i = 0; i < n_taps / 2; i++) {
        double d = fabs(h[i] - h[n_taps - 1 - i]);
        if (d > worst) worst = d;
    }
    return worst;
}

static void fir_complex(const double *h, int n_taps, double f, double fs, double *re, double *im) {
    double w = 2.0 * M_PI * f / fs;
    *re = 0.0;
    *im = 0.0;
    for (int i = 0; i < n_taps; i++) {
        *re += h[i] * cos(w * i);
        *im -= h[i] * sin(w * i);
    }
}

double fir_group_delay_spread(const double *h, int n_taps, double fs, double f_lo, double f_hi) {
    const double df = 1.0;
    double prev_phase = 0.0;
    double gd_min = 1e18, gd_max = -1e18;
    int first = 1;
    for (double f = f_lo; f <= f_hi; f += df) {
        double re0, im0, re1, im1;
        fir_complex(h, n_taps, f, fs, &re0, &im0);
        fir_complex(h, n_taps, f + df, fs, &re1, &im1);
        double ph0 = atan2(im0, re0);
        double ph1 = atan2(im1, re1);
        double dph = ph1 - ph0;
        /* unwrap */
        while (dph > M_PI) dph -= 2.0 * M_PI;
        while (dph < -M_PI) dph += 2.0 * M_PI;
        (void)prev_phase;
        /* group delay = -dphi/domega; domega = 2*pi*df/fs */
        double gd = -dph / (2.0 * M_PI * df / fs);
        if (first) {
            gd_min = gd_max = gd;
            first = 0;
        } else {
            if (gd < gd_min) gd_min = gd;
            if (gd > gd_max) gd_max = gd;
        }
    }
    return first ? 1e9 : (gd_max - gd_min);
}
