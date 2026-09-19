#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "conv.h"
#include "filters.h"
#include "pgm_io.h"
#include "wav_io.h"

static double wall_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return 1000.0 * c.QuadPart / f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000.0 * ts.tv_sec + 1e-6 * ts.tv_nsec;
#endif
}

static int g_pass, g_fail;

static void report(const char *name, double value, const char *op, double thr, int ok) {
    if (ok) {
        g_pass++;
        printf("[PASS] %s=%.6g (criterion %s %.6g)\n", name, value, op, thr);
    } else {
        g_fail++;
        printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", name, value, op, thr);
    }
}

static int test_conv(void) {
    const int n = 128, m = 33;
    double *x = (double *)malloc((size_t)n * sizeof(double));
    double *h = (double *)malloc((size_t)m * sizeof(double));
    double *y1 = (double *)malloc((size_t)(n + m - 1) * sizeof(double));
    double *y2 = (double *)malloc((size_t)(n + m - 1) * sizeof(double));
    if (!x || !h || !y1 || !y2) {
        free(x);
        free(h);
        free(y1);
        free(y2);
        return -1;
    }
    for (int i = 0; i < n; i++) x[i] = sin(0.07 * i) + 0.3 * cos(0.2 * i);
    for (int i = 0; i < m; i++) h[i] = exp(-0.1 * i) * cos(0.3 * i);
    conv_direct(x, n, h, m, y1);
    conv_fft(x, n, h, m, y2);
    double err = rel_max_err_vec(y2, y1, n + m - 1);
    report("fft_conv_rel_err", err, "<", 1e-9, err < 1e-9);
    free(x);
    free(h);
    free(y1);
    free(y2);

    /* roadmap exp 1: large-N timing comparison — FFT convolution must win */
    const int NL = 65536, ML = 2049;
    double *xl = malloc((size_t)NL * sizeof(double));
    double *hl = malloc((size_t)ML * sizeof(double));
    double *yl = malloc(((size_t)NL + ML - 1) * sizeof(double));
    if (!xl || !hl || !yl) {
        free(xl); free(hl); free(yl);
        return err < 1e-9 ? 0 : -1;
    }
    for (int i = 0; i < NL; i++) xl[i] = sin(0.0007 * i) + 0.3 * cos(0.013 * i);
    for (int i = 0; i < ML; i++) hl[i] = exp(-0.002 * i) * cos(0.02 * i);

    double t0 = wall_ms();
    conv_direct(xl, NL, hl, ML, yl);
    double t_dir = wall_ms() - t0;
    t0 = wall_ms();
    conv_fft(xl, NL, hl, ML, yl);
    double t_fft = wall_ms() - t0;
    double speedup = t_dir / (t_fft + 1e-9);
    printf("  info: large conv N=%d M=%d direct=%.2f ms fft=%.2f ms speedup=%.1fx\n",
           NL, ML, t_dir, t_fft, speedup);
    report("fft_conv_speedup", speedup, ">=", 2.0, speedup >= 2.0);
    free(xl);
    free(hl);
    free(yl);
    return (err < 1e-9 && speedup >= 2.0) ? 0 : -1;
}

/* roadmap exp 4: IIR lowpass cutoff sweep — poles must stay inside the unit
 * circle, the pole angle must track the design frequency, and the measured
 * -3 dB point must follow the sweep. */
static int test_iir_pole_sweep(void) {
    const double fs = 48000.0;
    const double fcs[] = {500.0, 1000.0, 2000.0, 4000.0};
    int all_ok = 1;
    double worst_dev = 0;
    for (int i = 0; i < 4; i++) {
        double f0 = fcs[i];
        biquad b;
        biquad_lowpass(&b, fs, f0, 0.707);
        /* poles of z^2 + a1 z + a2: complex pair, radius = sqrt(a2). The RBJ
         * cookbook defines Q via the analog prototype, so the pole ANGLE sits
         * slightly below w0 (≈ w0/sqrt(2) at Q=0.707 for small w0) — the
         * -3 dB point is what tracks f0, and that is what we gate on. */
        double radius = sqrt(b.a2);
        double angle = acos(-b.a1 / (2.0 * radius));
        double w0 = 2.0 * M_PI * f0 / fs;
        double fc = biquad_find_cutoff(&b, fs, f0);
        double dev = fabs(fc - f0) / f0;
        if (dev > worst_dev) worst_dev = dev;
        int ok = radius < 1.0 && angle > 0.0 && angle < w0 && dev < 0.02;
        printf("  info: f0=%.0f Hz pole r=%.4f angle=%.1f Hz-equiv (< f0, RBJ Q convention),"
               " measured -3dB=%.1f Hz %s\n",
               f0, radius, angle * fs / (2.0 * M_PI), fc, ok ? "ok" : "FAIL");
        if (!ok) all_ok = 0;
    }
    report("iir_poles_stable_and_track", all_ok ? 1.0 : 0.0, "==", 1.0, all_ok);
    report("iir_sweep_worst_cutoff_dev", worst_dev, "<", 0.02, worst_dev < 0.02);
    return all_ok ? 0 : -1;
}

static int test_biquad_lpf(void) {
    const double fs = 48000.0;
    const double f0 = 1000.0;
    biquad b;
    biquad_lowpass(&b, fs, f0, 0.707);
    double fc = biquad_find_cutoff(&b, fs, f0);
    double dev = fabs(fc - f0) / f0;
    report("biquad_lpf_cutoff_rel_dev", dev, "<", 0.02, dev < 0.02);
    printf("  info: designed f0=%.1f Hz, measured -3dB=%.2f Hz\n", f0, fc);
    return dev < 0.02 ? 0 : -1;
}

static int test_fir_linear_phase(void) {
    const double fs = 16000.0;
    const int taps_list[2] = {63, 64}; /* odd (type-I) and even (fixed center) */
    int all_ok = 1;
    for (int i = 0; i < 2; i++) {
        int taps = taps_list[i];
        double *h = (double *)malloc((size_t)taps * sizeof(double));
        if (!h) return -1;
        fir_design_lowpass(h, taps, fs, 2000.0);
        double asym = fir_impulse_asymmetry(h, taps);
        double spread = fir_group_delay_spread(h, taps, fs, 200.0, 1500.0);
        int ok_a = asym < 1e-12;
        /* even-tap FIR has a half-sample group delay; its spread across the
         * passband is still flat, so the same bound applies */
        int ok_g = spread <= 0.5; /* samples */
        printf("  info: %d-tap FIR asymmetry=%.2e group-delay spread=%.3f samples\n",
               taps, asym, spread);
        char n1[64], n2[64];
        snprintf(n1, sizeof(n1), "fir%d_impulse_asymmetry", taps);
        snprintf(n2, sizeof(n2), "fir%d_group_delay_spread", taps);
        report(n1, asym, "<", 1e-12, ok_a);
        report(n2, spread, "<=", 0.5, ok_g);
        if (!ok_a || !ok_g) all_ok = 0;
        free(h);
    }
    return all_ok ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab03 selftest ===\n");
    setvbuf(stdout, NULL, _IONBF, 0);
    if (test_conv() != 0) g_fail++;
    if (test_biquad_lpf() != 0) g_fail++;
    if (test_iir_pole_sweep() != 0) g_fail++;
    if (test_fir_linear_phase() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    const double fs = 44100.0;
    const int taps = 127;
    double *h = (double *)malloc((size_t)taps * sizeof(double));
    if (!h) return 1;
    fir_design_lowpass(h, taps, fs, 3000.0);

    int w = 512, hgt = 128;
    uint8_t *img = (uint8_t *)calloc((size_t)w * hgt, 1);
    if (img) {
        for (int col = 0; col < w; col++) {
            double f = (double)col / (double)(w - 1) * (fs * 0.5);
            double mag = fir_mag(h, taps, fs, f);
            double db = 20.0 * log10(mag + 1e-12);
            double u = (db + 60.0) / 60.0;
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            int row = (int)lrint((1.0 - u) * (hgt - 1));
            img[row * w + col] = 255;
            if (row + 1 < hgt) img[(row + 1) * w + col] = 180;
        }
        pgm_write("out/fir_lpf_response.pgm", img, w, hgt);
        free(img);
    }
    free(h);

    /* 3-band EQ demo on multi-tone */
    const int n = 44100;
    double *x = (double *)malloc((size_t)n * sizeof(double));
    double *y = (double *)malloc((size_t)n * sizeof(double));
    float *yf = (float *)malloc((size_t)n * sizeof(float));
    if (!x || !y || !yf) {
        free(x);
        free(y);
        free(yf);
        return 1;
    }
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        x[i] = 0.35 * sin(2 * M_PI * 200 * t) + 0.35 * sin(2 * M_PI * 1000 * t) +
               0.35 * sin(2 * M_PI * 5000 * t);
    }
    biquad low, peak, high;
    biquad_lowpass(&low, fs, 300.0, 0.7);
    biquad_peaking(&peak, fs, 1000.0, 1.0, 8.0);
    biquad_highshelf(&high, fs, 4000.0, 0.7, -6.0);
    biquad_process(&low, x, n, y);
    biquad_process(&peak, y, n, x);
    biquad_process(&high, x, n, y);
    for (int i = 0; i < n; i++) yf[i] = (float)(y[i] * 0.8);
    wav_data wd;
    if (wav_from_floats(yf, n, (uint32_t)fs, &wd) == 0) {
        wav_write("out/eq_demo.wav", &wd);
        wav_free(&wd);
    }
    free(x);
    free(y);
    free(yf);
    printf("generated artifacts under out/\n");
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_generate();
    printf("usage: %s --selftest | --generate\n", argv[0]);
    return 2;
}
