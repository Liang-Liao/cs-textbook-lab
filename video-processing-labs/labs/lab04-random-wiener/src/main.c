#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"
#include "noise.h"
#include "pgm_io.h"
#include "wav_io.h"
#include "wiener.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static int test_gauss_stats(void) {
    /* Roadmap bound |mu| < 0.05·sigma/sqrt(N) is a 0.05-SE bound: for an
     * unbiased generator a single draw of any size passes with only ~4%
     * probability, so the check would be a seed lottery. The hard gate is
     * therefore the 95% CI (|z| < 2) on the pooled z-score across 10
     * independent segments; the roadmap quantity is still printed per
     * segment so the normalized deviation is visible. */
    const size_t n = 20000;
    const int nseg = 10;
    double *x = malloc(n * 5 * sizeof(double)); /* sigma check draws 5n */
    if (!x) return -1;

    const int nbins = 64, w = 256, h = 128;
    int *hist = calloc((size_t)nbins, sizeof(int));

    double z_sum = 0, z_max = 0, pass_strict = 0;
    for (int s = 0; s < nseg; s++) {
        rng_seed((unsigned)(100 + s));
        rng_gauss(x, n, 0.0, 1.0);
        double mu = mean(x, n);
        double sd = sqrt(var(x, n, mu));
        double se = sd / sqrt((double)n);
        double z = mu / se;
        z_sum += z;
        if (fabs(z) > z_max) z_max = fabs(z);
        if (fabs(mu) < 0.05 * se) pass_strict += 1.0; /* roadmap-literal bound */
        printf("  info: seg %d mu=%+.6g sigma=%.6g z=%+.3f (roadmap |mu|<0.05SE: %s)\n",
               s, mu, sd, z, fabs(mu) < 0.05 * se ? "yes" : "no");
        for (size_t i = 0; i < n; i++) {
            int b = (int)((x[i] + 4.0) / 8.0 * nbins);
            if (b >= 0 && b < nbins) hist[b]++;
        }
    }
    double z_pool = z_sum / sqrt((double)nseg); /* pooled z over all segments */
    printf("  info: pooled z=%.3f over %d segments, max |z|=%.3f, strict-bound pass rate=%.0f%%\n",
           z_pool, nseg, z_max, 100.0 * pass_strict / nseg);
    int ok = fabs(z_pool) < 2.0 && z_max < 3.0;
    report("gauss_pooled_z", fabs(z_pool), "<", 2.0, fabs(z_pool) < 2.0);
    report("gauss_max_segment_z", z_max, "<", 3.0, z_max < 3.0);
    int ok_s = 1;
    /* sigma check on a fresh big draw */
    rng_seed(999u);
    rng_gauss(x, n * 5, 0.0, 1.0);
    double mu5 = mean(x, n * 5);
    double sd5 = sqrt(var(x, n * 5, mu5));
    ok_s = fabs(sd5 - 1.0) < 0.02;
    report("gauss_sigma_dev", fabs(sd5 - 1.0), "<", 0.02, ok_s);
    int hmax = 1;
    for (int i = 0; i < nbins; i++) if (hist[i] > hmax) hmax = hist[i];
    uint8_t *img = calloc((size_t)w * h, 1);
    if (img) {
        for (int c = 0; c < w; c++) {
            int b = c * nbins / w;
            int colh = hist[b] * h / hmax;
            for (int r = h - 1; r >= h - colh && r >= 0; r--) img[r * w + c] = 220;
            /* theory gaussian curve */
            double xv = -4.0 + 8.0 * (c + 0.5) / w;
            double dens = exp(-0.5 * xv * xv) / sqrt(2 * M_PI);
            double peak = 1.0 / sqrt(2 * M_PI);
            int tr = h - 1 - (int)(dens / peak * (h - 40));
            if (tr >= 0 && tr < h) {
                img[tr * w + c] = 255;
                if (tr + 1 < h) img[(tr + 1) * w + c] = 255;
            }
        }
        pgm_write("out/noise_hist.pgm", img, w, h);
        free(img);
    }
    free(hist);
    free(x);
    return (ok && ok_s) ? 0 : -1;
}

static void make_speechish(double *x, size_t n, double fs) {
    for (size_t i = 0; i < n; i++) {
        double t = (double)i / fs;
        double env = 0.5 + 0.5 * sin(2 * M_PI * 3.0 * t);
        x[i] = env * (0.6 * sin(2 * M_PI * 180 * t) + 0.3 * sin(2 * M_PI * 360 * t) +
                      0.15 * sin(2 * M_PI * 540 * t));
    }
}

static int test_wiener(void) {
    const double fs = 16000.0;
    const int N = 1024;
    const int n = N; /* single frame for oracle PSD */
    double *clean = malloc((size_t)n * sizeof(double));
    double *noisy = malloc((size_t)n * sizeof(double));
    double *out = malloc((size_t)n * sizeof(double));
    double *Ps = malloc((size_t)n * sizeof(double));
    double *Pn = malloc((size_t)n * sizeof(double));
    if (!clean || !noisy || !out || !Ps || !Pn) {
        free(clean); free(noisy); free(out); free(Ps); free(Pn);
        return -1;
    }
    make_speechish(clean, (size_t)n, fs);
    memcpy(noisy, clean, (size_t)n * sizeof(double));
    double pn = add_noise_at_snr(noisy, (size_t)n, 0.0, 7u);
    printf("  info: input SNR=0 dB, noise power=%.6g\n", pn);

    /* oracle PSD from clean and known white noise variance */
    cpx *C = calloc((size_t)n, sizeof(cpx));
    if (!C) { free(clean); free(noisy); free(out); free(Ps); free(Pn); return -1; }
    for (int i = 0; i < n; i++) { C[i].re = clean[i]; C[i].im = 0; }
    fft(C, n);
    for (int i = 0; i < n; i++) {
        Ps[i] = C[i].re * C[i].re + C[i].im * C[i].im;
        Pn[i] = pn * (double)n; /* white noise flat PSD scale */
    }
    apply_wiener_frame(noisy, Ps, Pn, n, out);

    double snr_in = snr_db(clean, noisy, (size_t)n);
    double snr_out = snr_db(clean, out, (size_t)n);
    double gain = snr_out - snr_in;
    printf("  info: SNR in=%.3f out=%.3f gain=%.3f dB\n", snr_in, snr_out, gain);
    report("wiener_snr_gain_db", gain, ">=", 3.0, gain >= 3.0);

    /* roadmap criterion, second half: WITHOUT prior (clean) information the
     * same Wiener filter degrades. Estimate Ps from the noisy spectrum by
     * subtracting the known noise power — no clean reference anywhere. */
    cpx *Y = calloc((size_t)n, sizeof(cpx));
    double *Ps_est = malloc((size_t)n * sizeof(double));
    double *out_est = malloc((size_t)n * sizeof(double));
    if (!Y || !Ps_est || !out_est) {
        free(Y); free(Ps_est); free(out_est);
        free(C); free(clean); free(noisy); free(out); free(Ps); free(Pn);
        return -1;
    }
    for (int i = 0; i < n; i++) { Y[i].re = noisy[i]; Y[i].im = 0; }
    fft(Y, n);
    for (int i = 0; i < n; i++) {
        double y2 = Y[i].re * Y[i].re + Y[i].im * Y[i].im;
        Ps_est[i] = fmax(y2 - Pn[i], 0.0);
    }
    apply_wiener_frame(noisy, Ps_est, Pn, n, out_est);
    double gain_est = snr_db(clean, out_est, (size_t)n) - snr_in;
    printf("  info: estimated-Ps gain=%.3f dB (oracle %.3f dB) — without prior info the\n"
           "        chi-square noise on Ps leaks straight into the gain, which is why the\n"
           "        roadmap gain figure requires known Ps/Pn\n",
           gain_est, gain);
    report("wiener_est_gain_db", gain_est, ">=", 0.0, gain_est >= 0.0);
    report("wiener_oracle_beats_est_db", gain - gain_est, ">=", 0.0, gain - gain_est >= 0.0);

    wav_data w;
    if (wav_from_doubles(clean, (size_t)n, (uint32_t)fs, &w) == 0) {
        wav_write("out/clean.wav", &w); wav_free(&w);
    }
    if (wav_from_doubles(noisy, (size_t)n, (uint32_t)fs, &w) == 0) {
        wav_write("out/noisy.wav", &w); wav_free(&w);
    }
    if (wav_from_doubles(out, (size_t)n, (uint32_t)fs, &w) == 0) {
        wav_write("out/wiener.wav", &w); wav_free(&w);
    }
    free(C); free(Y); free(Ps_est); free(out_est);
    free(clean); free(noisy); free(out); free(Ps); free(Pn);
    return (gain >= 3.0 && gain_est >= 0.0) ? 0 : -1;
}

static int test_ar1(void) {
    const size_t n = 8192;
    double *x = malloc(n * sizeof(double));
    if (!x) return -1;
    ar1(x, n, 0.9, 0.1, 99u);
    double mu = mean(x, n);
    double v = var(x, n, mu);
    int ok_var = v > 0.01 && v < 10.0;
    report("ar1_var", v, "in", 5.0, ok_var);

    /* power spectrum: AR(1) with a1=0.9 is low-pass; high/low ratio must be <<1 */
    /* average periodogram over 8 segments for lower variance */
    const int nseg = 8;
    size_t seg_len = n / (size_t)nseg;
    size_t half = seg_len / 2;
    double *psd = calloc(half, sizeof(double));
    if (!psd) { free(x); return -1; }
    for (int s = 0; s < nseg; s++) {
        cpx *S = calloc(seg_len, sizeof(cpx));
        if (!S) { free(psd); free(x); return -1; }
        for (size_t i = 0; i < seg_len; i++) {
            size_t idx = (size_t)s * seg_len + i;
            double win = 0.5 - 0.5 * cos(2.0 * M_PI * (double)i / (double)(seg_len - 1));
            S[i].re = x[idx] * win;
            S[i].im = 0.0;
        }
        fft(S, (int)seg_len);
        for (size_t k = 0; k < half; k++) {
            double m2 = S[k].re * S[k].re + S[k].im * S[k].im;
            psd[k] += m2 / (double)nseg;
        }
        free(S);
    }
    /* low-freq = bins [1, half/4), high-freq = bins [3*half/4, half) */
    size_t lo_lo = 1, lo_hi = half / 4;
    size_t hi_lo = 3 * half / 4, hi_hi = half;
    double e_lo = 0.0, e_hi = 0.0;
    for (size_t k = lo_lo; k < lo_hi; k++) e_lo += psd[k];
    for (size_t k = hi_lo; k < hi_hi; k++) e_hi += psd[k];
    double ratio = e_lo / (e_hi + 1e-30);
    /* AR(1) a1=0.9 theoretical ratio >> 1; white noise ≈ 1. Threshold at 4.0. */
    int ok_spec = ratio > 4.0;
    printf("  info: AR(1) a1=0.9 E_lo=%.6g E_hi=%.6g lo/hi=%.3f\n", e_lo, e_hi, ratio);
    report("ar1_lo_hi_ratio", ratio, ">", 4.0, ok_spec);

    /* write spectrum PGM */
    {
        const int w = 256, h = 128;
        uint8_t *img = calloc((size_t)w * h, 1);
        if (img) {
            double smax = 1e-300;
            for (size_t k = 0; k < half; k++) if (psd[k] > smax) smax = psd[k];
            for (int c = 0; c < w; c++) {
                size_t k = (size_t)c * (half - 1) / (size_t)(w - 1);
                double u = log10(psd[k] / smax + 1e-30) / 60.0 + 1.0; /* dB / 60 + 1 */
                if (u < 0) u = 0;
                if (u > 1) u = 1;
                int rh = (int)(u * (h - 1));
                for (int r = h - 1; r >= h - rh && r >= 0; r--) img[r * w + c] = 220;
            }
            pgm_write("out/ar1_spectrum.pgm", img, w, h);
            free(img);
        }
    }

    wav_data w;
    if (wav_from_doubles(x, n, 16000, &w) == 0) {
        wav_write("out/ar1.wav", &w); wav_free(&w);
    }
    free(psd);
    free(x);
    return (ok_var && ok_spec) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab04 selftest ===\n");
    if (test_gauss_stats() != 0) g_fail++;
    if (test_wiener() != 0) g_fail++;
    if (test_ar1() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    /* reuse selftest which already writes artifacts */
    return run_selftest();
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_generate();
    printf("usage: %s --selftest|--generate\n", argv[0]);
    return 2;
}
