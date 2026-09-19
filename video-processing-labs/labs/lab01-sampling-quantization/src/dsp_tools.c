#include "dsp_tools.h"

#include <math.h>
#include <stdlib.h>

void synth_multitone(float *out, size_t n, double fs, const double *freqs,
                     const double *amps, int n_tones) {
    for (size_t i = 0; i < n; i++) out[i] = 0.0f;
    double peak = 1e-12;
    for (size_t i = 0; i < n; i++) {
        double t = (double)i / fs;
        double s = 0.0;
        for (int k = 0; k < n_tones; k++) {
            s += amps[k] * sin(2.0 * M_PI * freqs[k] * t);
        }
        out[i] = (float)s;
        if (fabs(s) > peak) peak = fabs(s);
    }
    float scale = (float)(0.9 / peak);
    for (size_t i = 0; i < n; i++) out[i] *= scale;
}

void synth_sine(float *out, size_t n, double fs, double freq, double peak_amp) {
    for (size_t i = 0; i < n; i++) {
        double t = (double)i / fs;
        out[i] = (float)(peak_amp * sin(2.0 * M_PI * freq * t));
    }
}

double rms(const float *x, size_t n) {
    double acc = 0.0;
    for (size_t i = 0; i < n; i++) acc += (double)x[i] * (double)x[i];
    return sqrt(acc / (double)n);
}

double peak_abs(const float *x, size_t n) {
    double p = 0.0;
    for (size_t i = 0; i < n; i++) {
        double a = fabs((double)x[i]);
        if (a > p) p = a;
    }
    return p;
}

double quant_snr_theory_db(int bits) {
    return 6.02 * (double)bits + 1.76;
}

/* Signed mid-tread: code in [-2^{B-1}, 2^{B-1}-1], step = 2/2^B. */
static int quantize_code(double x, int bits) {
    double step = 2.0 / (double)(1u << bits);
    long code = lround(x / step);
    long lo = -(1L << (bits - 1));
    long hi = (1L << (bits - 1)) - 1;
    if (code < lo) code = lo;
    if (code > hi) code = hi;
    return (int)code;
}

static double code_to_float(int code, int bits) {
    double step = 2.0 / (double)(1u << bits);
    return (double)code * step;
}

double quantize_snr_db(const float *x, size_t n, int bits, float *y_out) {
    double sig = 0.0, err = 0.0;
    for (size_t i = 0; i < n; i++) {
        int code = quantize_code((double)x[i], bits);
        double y = code_to_float(code, bits);
        if (y_out) y_out[i] = (float)y;
        double e = (double)x[i] - y;
        sig += (double)x[i] * (double)x[i];
        err += e * e;
    }
    if (err <= 0.0) return 200.0;
    return 10.0 * log10(sig / err);
}

/* Double-precision SNR for full-scale theory check (no float storage error). */
double quantize_snr_db_double(const double *x, size_t n, int bits) {
    double sig = 0.0, err = 0.0;
    for (size_t i = 0; i < n; i++) {
        int code = quantize_code(x[i], bits);
        double y = code_to_float(code, bits);
        double e = x[i] - y;
        sig += x[i] * x[i];
        err += e * e;
    }
    if (err <= 0.0) return 200.0;
    return 10.0 * log10(sig / err);
}

void apply_dither(float *x, size_t n, double fs, int bits, unsigned seed) {
    (void)fs;
    int levels = 1 << bits;
    double lsb = 2.0 / (double)levels;
    double amp = 0.5 * lsb; /* TPDF range ±0.5 LSB */
    srand(seed);
    for (size_t i = 0; i < n; i++) {
        double u1 = (rand() / (double)RAND_MAX) * 2.0 - 1.0;
        double u2 = (rand() / (double)RAND_MAX) * 2.0 - 1.0;
        x[i] += (float)(amp * (u1 + u2) * 0.5);
    }
}

void dft_magnitude(const float *x, int nfft, float *mag) {
    for (int k = 0; k < nfft; k++) {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < nfft; n++) {
            double ang = -2.0 * M_PI * (double)k * (double)n / (double)nfft;
            re += (double)x[n] * cos(ang);
            im += (double)x[n] * sin(ang);
        }
        mag[k] = (float)sqrt(re * re + im * im);
    }
}

double alias_fold(double freq, double fs) {
    if (fs <= 0.0) return 0.0;
    double f = fmod(fabs(freq), fs);
    if (f > fs / 2.0) f = fs - f;
    return f;
}
