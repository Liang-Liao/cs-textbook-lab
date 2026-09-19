#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beam.h"
#include "pgm_io.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

/* Broadband-ish colored noise (stable PHAT peak). */
static void make_source(double *x, int n, unsigned seed) {
    unsigned s = seed;
    double y = 0;
    for (int i = 0; i < n; i++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        double e = ((s & 0xffffff) / 8388608.0) - 1.0;
        y = 0.85 * y + e;
        x[i] = y * 0.3;
    }
}

static void delay_signal(const double *x, double *y, int n, double delay_samp) {
    for (int i = 0; i < n; i++) {
        double p = (double)i - delay_samp;
        int j = (int)floor(p);
        double fr = p - j;
        double s0 = (j >= 0 && j < n) ? x[j] : 0.0;
        double s1 = (j + 1 >= 0 && j + 1 < n) ? x[j + 1] : 0.0;
        y[i] = (1.0 - fr) * s0 + fr * s1;
    }
}

static int test_gcc_doa(void) {
    /* DOA via GCC-PHAT (not time-domain xcorr_lag). */
    const double fs = 48000, c = 343.0, d = 0.10;
    const int n = 8192;
    int worst = 0;
    double max_err = 0;
    for (int deg = -60; deg <= 60; deg += 15) {
        double tau = d * sin(deg * M_PI / 180.0) / c;
        double *x = calloc((size_t)n, sizeof(double));
        double *y = calloc((size_t)n, sizeof(double));
        if (!x || !y) { free(x); free(y); return -1; }
        make_source(x, n, 100u + (unsigned)(deg + 60));
        delay_signal(x, y, n, tau * fs);

        double est = gcc_phat(x, y, n, 64);
        /* y delayed x by tau → PHAT peak at -tau */
        double sn = (-est) * c / (d * fs);
        if (sn > 1) sn = 1;
        if (sn < -1) sn = -1;
        double est_deg = asin(sn) * 180.0 / M_PI;
        double err = fabs(est_deg - deg);
        if (err > max_err) max_err = err;
        if (err > 5.0) worst++;
        printf("  info: src=%d tau_samp=%.3f gcc_phat=%.3f est_deg=%.2f err=%.2f\n",
               deg, tau * fs, est, est_deg, err);
        free(x); free(y);
    }
    int ok = (worst == 0);
    report("doa_max_err_deg", max_err, "<=", 5.0, ok);
    report("doa_fail_count", (double)worst, "==", 0.0, ok);
    return ok ? 0 : -1;
}

static int test_das4_beam(void) {
    /* 4-mic delay-and-sum: steer to look=0, target at +40°, interferer at -40°.
     * mic spectra carry the target source phase; interferer response is the
     * residual array factor with src_deg = -40 on the same mic data. */
    const int nch = 4;
    const double d = 0.05, c = 343.0, f = 2000.0, fs = 48000;
    const double look = 0.0, src = 40.0;
    double re[4], im[4];
    for (int m = 0; m < nch; m++) {
        double tau = m * d * sin(src * M_PI / 180.0) / c;
        double ph = -2 * M_PI * f * tau;
        re[m] = cos(ph);
        im[m] = sin(ph);
    }
    double g_src = beam_pattern_gain(re, im, nch, d, fs, c, f, look, src);
    double g_int = beam_pattern_gain(re, im, nch, d, fs, c, f, look, -src);
    double diff_db = 20 * log10((g_src + 1e-12) / (g_int + 1e-12));
    printf("  info: 4-mic DAS gain src=%.4f interf=%.4f diff=%.2f dB\n",
           g_src, g_int, diff_db);
    int ok = diff_db >= 6.0;
    report("das4_gain_diff_db", diff_db, ">=", 6.0, ok);
    return ok ? 0 : -1;
}

/* coherent amplitude of x at frequency f (single DFT bin) — the array factor
 * is a narrowband concept, so the target/interferer gain is measured at the
 * design frequency */
static double amp_at(const double *x, int n, double fs, double f) {
    double w = 2 * M_PI * f / fs;
    double re = 0, im = 0;
    for (int i = 0; i < n; i++) {
        re += x[i] * cos(w * i);
        im -= x[i] * sin(w * i);
    }
    return 2.0 * sqrt(re * re + im * im) / n;
}

/* Signal-level 4-mic delay-and-sum: real waveforms propagate to each mic
 * with fractional delays, the beamformer advances each channel by the look
 * direction's steering delay and sums. Two scenarios (target only /
 * interferer only) give the signal-level target-vs-interferer gain. */
static int test_das_signal_level(void) {
    const int nch = 4;
    const int n = 16384;
    const double fs = 48000, c = 343.0, d = 0.05, f = 2000.0;
    const double look = 40.0; /* steer toward the target */

    /* expected plane-wave residual for the off-axis interferer */
    double phi = 2 * M_PI * f * d * (sin(-look * M_PI / 180.0) - sin(look * M_PI / 180.0)) / c;
    double af = fabs(sin(nch * phi / 2.0) / (nch * sin(phi / 2.0)));
    printf("  info: theory array factor at interferer = %.4f (%.2f dB)\n",
           af, 20 * log10(af + 1e-12));

    double amps[2]; /* 0: target steered, 1: interferer steered */
    double refs[2];
    for (int scen = 0; scen < 2; scen++) {
        double src_deg = (scen == 0) ? look : -look;
        double *s = calloc((size_t)n, sizeof(double));
        double *mic[4];
        double *aligned = calloc((size_t)n, sizeof(double));
        double *out = calloc((size_t)n, sizeof(double));
        if (!s || !aligned || !out) {
            free(s); free(aligned); free(out); return -1;
        }
        make_source(s, n, 500u + (unsigned)scen * 37u);
        unsigned rs = 900u + (unsigned)scen;
        for (int m = 0; m < nch; m++) {
            mic[m] = calloc((size_t)n, sizeof(double));
            if (!mic[m]) { free(s); free(aligned); free(out); return -1; }
            double tau = m * d * sin(src_deg * M_PI / 180.0) / c;
            delay_signal(s, mic[m], n, tau * fs);
            for (int i = 0; i < n; i++) {
                rs = rs * 1103515245u + 12345u;
                mic[m][i] += 0.002 * (((rs >> 16) & 0x7fff) / 16384.0 - 1.0);
            }
            /* steer: advance channel m by the look-direction delay */
            double adv = -m * d * sin(look * M_PI / 180.0) / c * fs;
            delay_signal(mic[m], aligned, n, adv);
            for (int i = 0; i < n; i++) out[i] += aligned[i] / nch;
        }
        int lo = 2000, hi = n - 2000;
        amps[scen] = amp_at(out + lo, hi - lo, fs, f);
        refs[scen] = amp_at(s + lo, hi - lo, fs, f); /* mic 0 = unsteered ref */
        printf("  info: scenario %s: DAS amp@2kHz=%.4f (single-mic ref=%.4f)\n",
               scen == 0 ? "target    " : "interferer", amps[scen], refs[scen]);
        free(s);
        free(aligned);
        free(out);
        for (int m = 0; m < nch; m++) free(mic[m]);
    }
    double g_tgt = 20 * log10((amps[0] + 1e-30) / (refs[0] + 1e-30));
    double g_int = 20 * log10((amps[1] + 1e-30) / (refs[1] + 1e-30));
    double diff = g_tgt - g_int;
    printf("  info: signal-level DAS target gain=%.2f dB, interferer gain=%.2f dB, diff=%.2f dB\n",
           g_tgt, g_int, diff);
    report("das_signal_level_diff_db", diff, ">=", 6.0, diff >= 6.0);
    return diff >= 6.0 ? 0 : -1;
}

static int test_beam8(void) {
    /* Keep 8-el theory plot + gain check. */
    const int nch = 8;
    const double d = 0.08, c = 343.0, f = 2000.0, fs = 48000;
    double look = 0.0, src = 40.0;
    double re[8], im[8];
    for (int m = 0; m < nch; m++) {
        double tau = m * d * sin(src * M_PI / 180.0) / c;
        double ph = -2 * M_PI * f * tau;
        re[m] = cos(ph);
        im[m] = sin(ph);
    }
    double g_src = beam_pattern_gain(re, im, nch, d, fs, c, f, look, src);
    double g_null = beam_pattern_gain(re, im, nch, d, fs, c, f, look, -src);
    double diff_db = 20 * log10((g_src + 1e-12) / (g_null + 1e-12));
    printf("  info: 8-el beam gain src=%.4f other=%.4f diff=%.2f dB\n", g_src, g_null, diff_db);
    int ok = diff_db >= 6.0;
    report("beam8_gain_diff_db", diff_db, ">=", 6.0, ok);

    /* beam pattern PGM: 4-mic (primary experiment) and 8-el theory */
    int w = 181, h = 128;
    uint8_t *img = calloc((size_t)w * h, 1);
    if (img) {
        /* 4-mic pattern in top half */
        double re4[4], im4[4];
        double d4 = 0.05;
        for (int m = 0; m < 4; m++) {
            double tau = m * d4 * sin(src * M_PI / 180.0) / c;
            double ph = -2 * M_PI * f * tau;
            re4[m] = cos(ph);
            im4[m] = sin(ph);
        }
        for (int col = 0; col < w; col++) {
            double lookd = -90 + col;
            double g4 = beam_pattern_gain(re4, im4, 4, d4, fs, c, f, lookd, src);
            double g8 = beam_pattern_gain(re, im, 8, d, fs, c, f, lookd, src);
            double u4 = fabs(g4); if (u4 > 1) u4 = 1;
            double u8 = fabs(g8); if (u8 > 1) u8 = 1;
            int r4 = (int)((1.0 - u4) * (h / 2 - 1));
            int r8 = h / 2 + (int)((1.0 - u8) * (h / 2 - 1));
            img[r4 * w + col] = 255;
            img[r8 * w + col] = 255;
        }
        pgm_write("out/beam_pattern.pgm", img, w, h);
        free(img);
    }
    return ok ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab10 selftest ===\n");
    if (test_gcc_doa() != 0) g_fail++;
    if (test_das_signal_level() != 0) g_fail++;
    if (test_das4_beam() != 0) g_fail++;
    if (test_beam8() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && (!strcmp(argv[1], "--selftest") || !strcmp(argv[1], "--generate")))
        return run_selftest();
    printf("usage: %s --selftest\n", argv[0]);
    return 2;
}
