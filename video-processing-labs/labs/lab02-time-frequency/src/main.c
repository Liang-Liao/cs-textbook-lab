#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"
#include "pgm_io.h"
#include "stft.h"

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

static void fill_test_signal(cpx *x, int n) {
    for (int i = 0; i < n; i++) {
        double t = (double)i / (double)n;
        x[i].re = 0.7 * sin(2.0 * M_PI * 3.0 * t) + 0.3 * cos(2.0 * M_PI * 7.0 * t);
        x[i].im = 0.2 * sin(2.0 * M_PI * 5.0 * t);
    }
}

static int test_fft_vs_dft(void) {
    const int n = 64;
    cpx *x = (cpx *)malloc((size_t)n * sizeof(cpx));
    cpx *a = (cpx *)malloc((size_t)n * sizeof(cpx));
    cpx *b = (cpx *)malloc((size_t)n * sizeof(cpx));
    if (!x || !a || !b) {
        free(x);
        free(a);
        free(b);
        return -1;
    }
    fill_test_signal(x, n);
    dft(x, a, n);
    memcpy(b, x, (size_t)n * sizeof(cpx));
    fft(b, n);
    double err = rel_max_err(b, a, n);
    report("fft_vs_dft_rel_err", err, "<", 1e-9, err < 1e-9);
    free(x);
    free(a);
    free(b);
    return err < 1e-9 ? 0 : -1;
}

static int test_fft_speed(void) {
    const int n = 1024;
    cpx *x = (cpx *)malloc((size_t)n * sizeof(cpx));
    cpx *a = (cpx *)malloc((size_t)n * sizeof(cpx));
    cpx *b = (cpx *)malloc((size_t)n * sizeof(cpx));
    if (!x || !a || !b) {
        free(x);
        free(a);
        free(b);
        return -1;
    }
    fill_test_signal(x, n);

    double t0 = wall_ms();
    dft(x, a, n);
    double t_dft = wall_ms() - t0;

    memcpy(b, x, (size_t)n * sizeof(cpx));
    t0 = wall_ms();
    /* warmup + timed */
    fft(b, n);
    memcpy(b, x, (size_t)n * sizeof(cpx));
    t0 = wall_ms();
    fft(b, n);
    double t_fft = wall_ms() - t0;

    /* require fft < dft/50; guard tiny timings */
    double ratio = (t_fft > 1e-6) ? (t_dft / t_fft) : 1e9;
    int ok = ratio >= 50.0;
    printf("  info: DFT %.3f ms, FFT %.3f ms, speedup %.1fx\n", t_dft, t_fft, ratio);
    report("fft_speedup", ratio, ">=", 50.0, ok);
    free(x);
    free(a);
    free(b);
    return ok ? 0 : -1;
}

static int test_stft_roundtrip(void) {
    const int nfft = 256;
    const int hop = nfft / 2;
    const int n_sig = nfft * 8;
    const int max_frames = 1 + (n_sig - nfft) / hop;
    double *sig = (double *)malloc((size_t)n_sig * sizeof(double));
    double *rec = (double *)malloc((size_t)n_sig * sizeof(double));
    cpx *frames = (cpx *)malloc((size_t)max_frames * (size_t)nfft * sizeof(cpx));
    if (!sig || !rec || !frames) {
        free(sig);
        free(rec);
        free(frames);
        return -1;
    }
    for (int i = 0; i < n_sig; i++) {
        double t = (double)i / 16000.0;
        sig[i] = 0.5 * sin(2.0 * M_PI * 440.0 * t) + 0.3 * sin(2.0 * M_PI * 1200.0 * t);
    }
    int n_frames = 0;
    stft_analyze(sig, n_sig, nfft, hop, frames, &n_frames);
    stft_synthesize(frames, n_frames, nfft, hop, rec, n_sig);

    int lo = hop, hi = n_sig - hop;
    double e_ref = 0.0, e_err = 0.0;
    for (int i = lo; i < hi; i++) {
        double d = rec[i] - sig[i];
        e_ref += sig[i] * sig[i];
        e_err += d * d;
    }
    double rel = (e_ref > 0) ? sqrt(e_err / e_ref) : 0.0;
    report("stft_roundtrip_rel_err", rel, "<", 1e-10, rel < 1e-10);

    free(sig);
    free(rec);
    free(frames);
    return rel < 1e-10 ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab02 selftest ===\n");
    if (test_fft_vs_dft() != 0) g_fail++;
    if (test_fft_speed() != 0) g_fail++;
    if (test_stft_roundtrip() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    const int n_sig = 16000;
    const int nfft = 512;
    const int hop = 128;
    float *sig = (float *)malloc((size_t)n_sig * sizeof(float));
    if (!sig) return 1;
    /* linear chirp 200 → 3500 Hz at fs=8k */
    for (int i = 0; i < n_sig; i++) {
        double t = (double)i / 8000.0;
        double f0 = 200.0, f1 = 3500.0, T = (double)n_sig / 8000.0;
        double phase = 2.0 * M_PI * (f0 * t + (f1 - f0) * t * t / (2.0 * T));
        sig[i] = (float)(0.8 * sin(phase));
    }
    int w = 512, h = 256;
    uint8_t *img = (uint8_t *)calloc((size_t)w * h, 1);
    if (img) {
        spectrogram_to_pgm(sig, n_sig, nfft, hop, img, w, h);
        pgm_write("out/sweep_spectrogram.pgm", img, w, h);
        free(img);
    }
    /* multi-segment "bird" tracks */
    for (int i = 0; i < n_sig; i++) sig[i] = 0.0f;
    for (int seg = 0; seg < 4; seg++) {
        int start = seg * n_sig / 4;
        int len = n_sig / 4;
        double f0 = 500.0 + 400.0 * seg;
        for (int i = 0; i < len; i++) {
            double t = (double)i / 8000.0;
            double f = f0 + 800.0 * t;
            sig[start + i] = (float)(0.7 * sin(2.0 * M_PI * f * t));
        }
    }
    img = (uint8_t *)calloc((size_t)w * h, 1);
    if (img) {
        spectrogram_to_pgm(sig, n_sig, nfft, hop, img, w, h);
        pgm_write("out/chirp_bird.pgm", img, w, h);
        free(img);
    }
    free(sig);
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
