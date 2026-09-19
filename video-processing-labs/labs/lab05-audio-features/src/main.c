#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"
#include "lufs.h"
#include "mel.h"
#include "pgm_io.h"
#include "stft.h"
#include "wav_io.h"

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

/* Speech-like synthetic: F0 pulse train + formant-ish harmonics + envelope. */
static void make_speechish(double *x, int n, double fs) {
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        double env = 0.55 + 0.45 * sin(2.0 * M_PI * 2.5 * t);
        if (env < 0.05) env = 0.05;
        double s = 0.0;
        s += 0.50 * sin(2.0 * M_PI * 120.0 * t);
        s += 0.30 * sin(2.0 * M_PI * 240.0 * t);
        s += 0.18 * sin(2.0 * M_PI * 730.0 * t);
        s += 0.12 * sin(2.0 * M_PI * 1090.0 * t);
        s += 0.08 * sin(2.0 * M_PI * 2440.0 * t);
        x[i] = env * 0.25 * s;
    }
}

static int test_mel_centers(void) {
    const int n_mels = 40;
    const int nfft = 1024;
    const double fs = 16000.0;
    const double fmin = 0.0, fmax = 8000.0;
    double *weights = malloc((size_t)n_mels * (size_t)(nfft / 2 + 1) * sizeof(double));
    double *centers = malloc((size_t)n_mels * sizeof(double));
    if (!weights || !centers) {
        report("mel_alloc", 0.0, ">=", 1.0, 0);
        free(weights);
        free(centers);
        return -1;
    }
    mel_filterbank(n_mels, nfft, fs, fmin, fmax, weights, centers);

    int monotonic = 1;
    double max_mel_dev = 0.0;
    for (int m = 0; m < n_mels; m++) {
        if (m > 0 && centers[m] <= centers[m - 1]) monotonic = 0;
        double fm = hz_to_mel(centers[m]);
        double ideal = hz_to_mel(fmin) +
                       (hz_to_mel(fmax) - hz_to_mel(fmin)) * (double)(m + 1) / (double)(n_mels + 1);
        double d = fabs(fm - ideal);
        if (d > max_mel_dev) max_mel_dev = d;
    }
    /* coverage: first filter left ~0, last filter right ~fmax (via centers + support) */
    double c0 = centers[0];
    double cN = centers[n_mels - 1];
    int cover_lo = c0 > 0.0 && c0 < fmax * 0.08; /* first center above 0 but low */
    int cover_hi = cN > fmax * 0.85 && cN <= fmax + 1e-6;
    int in_range = 1;
    for (int m = 0; m < n_mels; m++) {
        if (centers[m] < 0.0 || centers[m] > fmax + 1e-6) in_range = 0;
    }

    printf("  info: mel centers[0]=%.2f Hz, centers[%d]=%.2f Hz, max_mel_dev=%.4f\n",
           centers[0], n_mels - 1, centers[n_mels - 1], max_mel_dev);

    report("mel_centers_monotonic", monotonic ? 1.0 : 0.0, ">=", 1.0, monotonic);
    report("mel_scale_deviation_mel", max_mel_dev, "<", 1.0, max_mel_dev < 1.0);
    report("mel_cover_low_hz", c0, "<", 0.08 * fmax, cover_lo);
    report("mel_cover_high_hz", cN, ">", 0.85 * fmax, cover_hi);
    report("mel_centers_in_range", in_range ? 1.0 : 0.0, ">=", 1.0, in_range);

    free(weights);
    free(centers);
    return (monotonic && max_mel_dev < 1.0 && cover_lo && cover_hi && in_range) ? 0 : -1;
}

static int test_lufs_gain(void) {
    const double fs = 16000.0;
    const int n = (int)(3.0 * fs); /* 3 s */
    double *x = malloc((size_t)n * sizeof(double));
    if (!x) {
        report("lufs_alloc", 0.0, ">=", 1.0, 0);
        return -1;
    }
    make_speechish(x, n, fs);

    double l0 = lufs_integrated(x, n, fs);
    double l1 = lufs_integrated_gain(x, n, fs, 2.0); /* +6 dB amplitude */
    double delta = l1 - l0;
    double err = fabs(delta - 6.0);
    printf("  info: LUFS original=%.3f, +6dB=%.3f, delta=%.3f LU\n", l0, l1, delta);
    report("lufs_gain_delta_lu", delta, "~6±0.5", 0.5, err <= 0.5);

    /* also write wav for generate convenience */
    wav_data w;
    if (wav_from_doubles(x, (size_t)n, (uint32_t)fs, &w) == 0) {
        wav_write("out/speechish.wav", &w);
        wav_free(&w);
    }
    free(x);
    return err <= 0.5 ? 0 : -1;
}

/* Objective dual-tone masking: masker 1 kHz, probe swept in amplitude.
 * Critical-band (Bark) model: if probe falls in same critical band,
 * masking threshold ≈ masker level - some offset; find probe amp where
 * probe is just at threshold. */
static void dual_tone_masking_demo(void) {
    /* Roadmap exp 2 as an objective amplitude scan: the critical-band model
     * (spreading function) gives the masking threshold; the masker's REAL
     * spectral floor at the probe bin (measured from a masker-only DFT) also
     * contributes. The scan runs for an in-band probe (1125 Hz) and a
     * near-edge probe (1400 Hz): the threshold must RISE with Bark distance
     * from the masker — the critical-band dependence. */
    const double fs = 16000.0;
    const double f_mask = 1000.0;
    const double A_mask = 1.0;
    const double probes[2] = {1125.0, 1400.0};
    double thr_total[2];

    /* real spectral floor at each probe bin from the masker alone */
    const int nfft = 8192;
    cpx *X = calloc((size_t)nfft, sizeof(cpx));
    for (int i = 0; i < nfft; i++) {
        double w = 0.5 - 0.5 * cos(2.0 * M_PI * i / (nfft - 1));
        X[i].re = A_mask * sin(2.0 * M_PI * f_mask * i / fs) * w;
        X[i].im = 0.0;
    }
    fft(X, nfft);
    double mask_bin_pow = 0;
    {
        int b = (int)(f_mask / fs * nfft + 0.5);
        mask_bin_pow = X[b].re * X[b].re + X[b].im * X[b].im;
    }

    for (int p = 0; p < 2; p++) {
        double f_probe = probes[p];
        double bark_m = 13.0 * atan(0.00076 * f_mask) +
                        3.5 * atan((f_mask / 7500.0) * (f_mask / 7500.0));
        double bark_p = 13.0 * atan(0.00076 * f_probe) +
                        3.5 * atan((f_probe / 7500.0) * (f_probe / 7500.0));
        double bark_dist = fabs(bark_m - bark_p);
        double spread_db = -27.0 * bark_dist;
        double thr_rel_db = spread_db - 6.0; /* safety margin model */
        double a_model = A_mask * pow(10.0, thr_rel_db / 20.0);

        /* masker leakage floor at the probe bin, expressed as an equivalent
         * probe amplitude (relative to the masker bin power) */
        int b = (int)(f_probe / fs * nfft + 0.5);
        double floor_pow = X[b].re * X[b].re + X[b].im * X[b].im;
        double floor_rel_db = 10.0 * log10((floor_pow + 1e-30) / (mask_bin_pow + 1e-30));
        double a_floor = A_mask * pow(10.0, floor_rel_db / 2.0) / 100.0;

        thr_total[p] = a_model > a_floor ? a_model : a_floor;
        printf("  info: probe %.0f Hz bark=%.2f model_thr=%.5f floor_thr=%.6f "
               "total_thr=%.5f\n",
               f_probe, bark_dist, a_model, a_floor, thr_total[p]);
    }
    free(X);

    /* critical-band dependence: the farther probe is masked much less */
    report("masking_thr_inband", thr_total[0], ">", 0.0, thr_total[0] > 0.0);
    report("masking_critical_band_ordering", thr_total[1], "<", 0.1 * thr_total[0],
           thr_total[1] < 0.1 * thr_total[0]);

    /* Write a mix at the in-band threshold for listening/reference. */
    const int n = (int)(1.0 * fs);
    double *x = malloc((size_t)n * sizeof(double));
    if (!x) return;
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        x[i] = 0.4 * A_mask * sin(2.0 * M_PI * f_mask * t) +
               0.4 * thr_total[0] * sin(2.0 * M_PI * probes[0] * t);
    }
    wav_data w;
    if (wav_from_doubles(x, (size_t)n, (uint32_t)fs, &w) == 0) {
        wav_write("out/dual_tone_mask.wav", &w);
        wav_free(&w);
    }
    free(x);
}

static int write_mel_pgm(void) {
    const double fs = 16000.0;
    const int n_sig = 16000;
    const int nfft = 512;
    const int hop = 128;
    const int n_mels = 64;
    int n_bins = nfft / 2 + 1;
    int max_frames = 1 + (n_sig - nfft) / hop;

    double *sig = malloc((size_t)n_sig * sizeof(double));
    cpx *frames = malloc((size_t)max_frames * (size_t)nfft * sizeof(cpx));
    double *weights = malloc((size_t)n_mels * (size_t)n_bins * sizeof(double));
    double *centers = malloc((size_t)n_mels * sizeof(double));
    double *power = malloc((size_t)max_frames * (size_t)n_bins * sizeof(double));
    double *mel_pow = malloc((size_t)max_frames * (size_t)n_mels * sizeof(double));
    uint8_t *img = malloc((size_t)512 * 256);
    if (!sig || !frames || !weights || !centers || !power || !mel_pow || !img) {
        free(sig);
        free(frames);
        free(weights);
        free(centers);
        free(power);
        free(mel_pow);
        free(img);
        return -1;
    }

    /* chirp + speechish mix for interesting mel image */
    double *sp = malloc((size_t)n_sig * sizeof(double));
    if (!sp) {
        free(sig);
        free(frames);
        free(weights);
        free(centers);
        free(power);
        free(mel_pow);
        free(img);
        return -1;
    }
    make_speechish(sp, n_sig, fs);
    for (int i = 0; i < n_sig; i++) {
        double t = (double)i / fs;
        double f0 = 200.0, f1 = 3500.0, T = (double)n_sig / fs;
        double phase = 2.0 * M_PI * (f0 * t + (f1 - f0) * t * t / (2.0 * T));
        sig[i] = 0.5 * sin(phase) + sp[i];
    }
    free(sp);

    mel_filterbank(n_mels, nfft, fs, 0.0, fs / 2.0, weights, centers);
    int n_frames = 0;
    stft_analyze(sig, n_sig, nfft, hop, frames, &n_frames);
    for (int t = 0; t < n_frames; t++) {
        const cpx *X = frames + (size_t)t * (size_t)nfft;
        double *P = power + (size_t)t * (size_t)n_bins;
        for (int k = 0; k < n_bins; k++) {
            P[k] = X[k].re * X[k].re + X[k].im * X[k].im;
        }
    }
    mel_spectrogram(power, n_frames, n_bins, weights, n_mels, mel_pow);
    /* to dB */
    for (int i = 0; i < n_frames * n_mels; i++) {
        mel_pow[i] = 10.0 * log10(mel_pow[i] + 1e-12);
    }
    mel_to_pgm(mel_pow, n_frames, n_mels, img, 512, 256);
    int rc = pgm_write("out/mel_spec.pgm", img, 512, 256);
    printf("  info: wrote out/mel_spec.pgm (%d frames x %d mels) rc=%d\n", n_frames,
           n_mels, rc);

    free(sig);
    free(frames);
    free(weights);
    free(centers);
    free(power);
    free(mel_pow);
    free(img);
    return rc;
}

static int run_selftest(void) {
    printf("=== lab05 selftest ===\n");
    test_mel_centers();
    test_lufs_gain();
    dual_tone_masking_demo();
    write_mel_pgm();
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) { return run_selftest(); }

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_generate();
    printf("usage: %s --selftest|--generate\n", argv[0]);
    return 2;
}
