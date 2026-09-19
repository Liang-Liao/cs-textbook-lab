#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"
#include "noise.h"
#include "ns.h"
#include "pgm_io.h"
#include "stft.h"
#include "wav_io.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static void make_clean(double *x, int n, double fs) {
    for (int i = 0; i < n; i++) {
        double t = (double)i / fs;
        double env = (t < 0.08 || t > 1.5) ? 0.0 : 1.0;
        /* smooth edges */
        if (t >= 0.08 && t < 0.10) env = (t - 0.08) / 0.02;
        if (t > 1.48 && t <= 1.5) env = (1.5 - t) / 0.02;
        x[i] = env * (0.5 * sin(2*M_PI*200*t) + 0.3 * sin(2*M_PI*400*t) + 0.2 * sin(2*M_PI*800*t));
    }
}

static void make_noisy(double *clean, double *noisy, int n, double snr_in_db, unsigned seed) {
    memcpy(noisy, clean, (size_t)n * sizeof(double));
    double ps = sig_power(clean, (size_t)n);
    double pn = ps / pow(10.0, snr_in_db / 10.0);
    add_white(noisy, (size_t)n, sqrt(pn), seed);
}

static void write_spectrograms(const double *noisy, const double *out, int n,
                               int nfft, int hop) {
    int w = 512, h = 256;
    uint8_t *img = calloc((size_t)w * h, 1);
    if (!img) return;
    spectrogram_to_pgm(noisy, n, nfft, hop, img, w, h);
    pgm_write("out/spec_before.pgm", img, w, h);
    spectrogram_to_pgm(out, n, nfft, hop, img, w, h);
    pgm_write("out/spec_after.pgm", img, w, h);
    free(img);
}

static int test_wiener_ns(void) {
    /* Main NS path: noise PSD from noisy only (no clean oracle). */
    const double fs = 16000.0;
    const int n = 32000;
    const int nfft = 256, hop = 128;
    double *clean = malloc((size_t)n * sizeof(double));
    double *noisy = malloc((size_t)n * sizeof(double));
    double *out = malloc((size_t)n * sizeof(double));
    double *N2 = malloc((size_t)nfft * sizeof(double));
    if (!clean || !noisy || !out || !N2) {
        free(clean); free(noisy); free(out); free(N2); return -1;
    }
    make_clean(clean, n, fs);
    make_noisy(clean, noisy, n, 5.0, 11u);

    /* noise estimate from noisy only — first 5 frames + minstat */
    ns_estimate_noise(noisy, n, nfft, hop, 5, N2);

    int maxf = 1 + (n - nfft) / hop;
    cpx *frames = malloc((size_t)maxf * nfft * sizeof(cpx));
    if (!frames) { free(clean); free(noisy); free(out); free(N2); return -1; }
    int nf = 0;
    stft_analyze(noisy, n, nfft, hop, frames, &nf);
    ns_process_frames(frames, nf, nfft, N2, 0.03);
    stft_synthesize(frames, nf, nfft, hop, out, n);

    double snr_in = snr_db(clean, noisy, n);
    double snr_out = snr_db(clean, out, n);
    double dmg = speech_damage_db(clean, noisy, out, n, nfft, hop);
    printf("  info: Wiener NS SNR in=%.3f out=%.3f speech_damage=%.3f dB\n",
           snr_in, snr_out, dmg);
    report("wiener_snr_db", snr_out, ">=", 10.0, snr_out >= 10.0);
    report("wiener_speech_damage_db", dmg, ">=", -1.0, dmg >= -1.0);

    wav_data w;
    if (wav_from_doubles(noisy, (size_t)n, (uint32_t)fs, &w)==0){ wav_write("out/noisy.wav",&w); wav_free(&w);}
    if (wav_from_doubles(out, (size_t)n, (uint32_t)fs, &w)==0){ wav_write("out/enhanced.wav",&w); wav_free(&w);}
    write_spectrograms(noisy, out, n, nfft, hop);

    free(frames); free(clean); free(noisy); free(out); free(N2);
    return (snr_out >= 10.0 && dmg >= -1.0) ? 0 : -1;
}

static int test_spectral_sub(void) {
    const double fs = 16000.0;
    const int n = 32000;
    const int nfft = 256, hop = 128;
    double *clean = malloc((size_t)n * sizeof(double));
    double *noisy = malloc((size_t)n * sizeof(double));
    double *out = malloc((size_t)n * sizeof(double));
    double *N2 = malloc((size_t)nfft * sizeof(double));
    if (!clean || !noisy || !out || !N2) {
        free(clean); free(noisy); free(out); free(N2); return -1;
    }
    make_clean(clean, n, fs);
    make_noisy(clean, noisy, n, 5.0, 11u);
    ns_estimate_noise(noisy, n, nfft, hop, 5, N2);

    int maxf = 1 + (n - nfft) / hop;
    cpx *frames = malloc((size_t)maxf * nfft * sizeof(cpx));
    if (!frames) { free(clean); free(noisy); free(out); free(N2); return -1; }
    int nf = 0;
    stft_analyze(noisy, n, nfft, hop, frames, &nf);
    ns_process_spectral_sub(frames, nf, nfft, N2, 1.2, 0.12);
    stft_synthesize(frames, nf, nfft, hop, out, n);

    double snr_in = snr_db(clean, noisy, n);
    double snr_out = snr_db(clean, out, n);
    printf("  info: spectral-sub SNR in=%.3f out=%.3f\n", snr_in, snr_out);
    report("specsub_snr_db", snr_out, ">=", 10.0, snr_out >= 10.0);

    /* roadmap exp 1: sweep alpha/beta and quantify the music-noise trade-off.
     * Musicality proxy: spectral flatness (geometric/arithmetic mean) of the
     * noise-only tail's average spectrum — musical noise carves deep holes
     * between remaining spikes, lowering flatness; smooth noise ≈ 1. */
    const double alphas[3] = {0.8, 1.2, 1.6};
    const double betas[3] = {0.05, 0.12, 0.2};
    double flat[3][3];
    FILE *csv = fopen("out/specsub_sweep.csv", "wb");
    if (csv) fprintf(csv, "alpha,beta,snr_out_db,spec_flatness_tail\n");
    int tail0 = (int)(1.6 * fs);
    for (int ia = 0; ia < 3; ia++) {
        for (int ib = 0; ib < 3; ib++) {
            stft_analyze(noisy, n, nfft, hop, frames, &nf);
            ns_process_spectral_sub(frames, nf, nfft, N2, alphas[ia], betas[ib]);
            stft_synthesize(frames, nf, nfft, hop, out, n);
            double so = snr_db(clean, out, n);
            /* average spectrum of the tail */
            double *P = calloc((size_t)nfft, sizeof(double));
            cpx *tf = malloc((size_t)(maxf) * nfft * sizeof(cpx));
            int tnf = 0;
            stft_analyze(out + tail0, n - tail0, nfft, hop, tf, &tnf);
            for (int f = 0; f < tnf; f++)
                for (int k = 0; k < nfft; k++)
                    P[k] += tf[(size_t)f * nfft + k].re * tf[(size_t)f * nfft + k].re +
                            tf[(size_t)f * nfft + k].im * tf[(size_t)f * nfft + k].im;
            double lg = 0, am = 0;
            int nb = nfft / 2;
            for (int k = 0; k < nb; k++) {
                lg += log(P[k] / tnf + 1e-30);
                am += P[k] / tnf;
            }
            double geo = exp(lg / nb);
            double ari = am / nb;
            flat[ia][ib] = geo / (ari + 1e-30);
            if (csv) fprintf(csv, "%.2f,%.2f,%.3f,%.4f\n", alphas[ia], betas[ib], so, flat[ia][ib]);
            free(P);
            free(tf);
        }
    }
    if (csv) fclose(csv);
    for (int ia = 0; ia < 3; ia++) {
        printf("  info: alpha=%.1f -> flatness beta0.05=%.3f beta0.12=%.3f beta0.2=%.3f\n",
               alphas[ia], flat[ia][0], flat[ia][1], flat[ia][2]);
    }
    /* trend: aggressive subtraction (high alpha, low beta) carves the deepest
     * holes → lowest flatness; gentle (low alpha, high beta) stays smoothest */
    int ok_trend = flat[2][0] < flat[0][2];
    report("specsub_music_trend", flat[2][0], "<", flat[0][2], ok_trend);

    free(frames); free(clean); free(noisy); free(out); free(N2);
    return snr_out >= 10.0 ? 0 : -1;
}

static int test_webrtc_ns(void) {
    const double fs = 16000.0;
    const int n = 32000;
    const int nfft = 256, hop = 128;
    double *clean = malloc((size_t)n * sizeof(double));
    double *noisy = malloc((size_t)n * sizeof(double));
    double *out = malloc((size_t)n * sizeof(double));
    if (!clean || !noisy || !out) {
        free(clean); free(noisy); free(out); return -1;
    }
    make_clean(clean, n, fs);
    make_noisy(clean, noisy, n, 5.0, 11u);

    int maxf = 1 + (n - nfft) / hop;
    cpx *frames = malloc((size_t)maxf * nfft * sizeof(cpx));
    if (!frames) { free(clean); free(noisy); free(out); return -1; }
    int nf = 0;
    stft_analyze(noisy, n, nfft, hop, frames, &nf);
    int nsp = ns_process_webrtc(frames, nf, nfft, 0.03);
    stft_synthesize(frames, nf, nfft, hop, out, n);

    double snr_in = snr_db(clean, noisy, n);
    double snr_out = snr_db(clean, out, n);
    double dmg = speech_damage_db(clean, noisy, out, n, nfft, hop);
    double speech_frac = nf ? (100.0 * nsp / nf) : 0.0;
    printf("  info: WebRTC-style NS SNR in=%.3f out=%.3f damage=%.3f speech=%.1f%% of frames\n",
           snr_in, snr_out, dmg, speech_frac);
    report("webrtc_snr_db", snr_out, ">=", 10.0, snr_out >= 10.0);
    report("webrtc_speech_damage_db", dmg, ">=", -1.0, dmg >= -1.0);
    /* LR speech detector sanity: speech occupies 0.08..1.5 s of the 2 s corpus
     * (~71%); a detector output far outside 30..95% means it is not discriminating */
    int ok_lr = speech_frac > 30.0 && speech_frac < 95.0;
    report("webrtc_lr_speech_pct", speech_frac, "in", 30.0, ok_lr);

    free(frames); free(clean); free(noisy); free(out);
    return (snr_out >= 10.0 && dmg >= -1.0 && ok_lr) ? 0 : -1;
}

static int test_minstat_track(void) {
    /* noise floor rises +10 dB midway; minstat should follow within ~1s */
    const int nfft = 256, hop = 128;
    const double fs = 16000.0;
    const int n = (int)fs * 2; /* 2 seconds */
    double *x = calloc((size_t)n, sizeof(double));
    double *N2 = calloc((size_t)nfft, sizeof(double));
    minstat ms;
    minstat_init(&ms, nfft, 40);
    if (!x || !N2 || !ms.buf) { free(x); free(N2); minstat_free(&ms); return -1; }

    /* only noise: first second low, second high */
    add_white(x, (size_t)n, 0.05, 3u);
    for (int i = n / 2; i < n; i++) x[i] *= sqrt(10.0); /* +10 dB */

    int maxf = 1 + (n - nfft) / hop;
    cpx *frames = malloc((size_t)maxf * nfft * sizeof(cpx));
    int nf = 0;
    stft_analyze(x, n, nfft, hop, frames, &nf);
    /* track energy near mid bin */
    int k0 = nfft / 8;
    double est_at_1s = 0, est_at_19s = 0;
    double *Y2 = malloc((size_t)nfft * sizeof(double));
    if (!Y2) { free(frames); free(x); free(N2); minstat_free(&ms); return -1; }
    for (int f = 0; f < nf; f++) {
        cpx *X = frames + (size_t)f * nfft;
        for (int k = 0; k < nfft; k++) Y2[k] = X[k].re*X[k].re + X[k].im*X[k].im;
        minstat_update(&ms, Y2);
        double tsec = (double)(f * hop) / fs;
        if (tsec > 0.9 && tsec < 1.0) est_at_1s = ms.N2[k0];
        if (tsec > 1.9) est_at_19s = ms.N2[k0];
    }
    free(Y2);
    double ratio = (est_at_1s > 1e-30) ? (est_at_19s / est_at_1s) : 0;
    double ratio_db = 10.0 * log10(ratio + 1e-30);
    printf("  info: minstat noise power ratio after step = %.2f dB\n", ratio_db);
    /* track the 10 dB jump within the remaining 1 s. The 40-frame (0.32 s)
     * window needs to refresh with the new level first, so ~9.5 dB is the
     * honest bound for this tracker design in the 1 s budget. */
    int ok = ratio_db >= 9.0;
    report("minstat_track_db", ratio_db, ">=", 9.0, ok);

    free(frames); free(x); free(N2); minstat_free(&ms);
    return ok ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab07 selftest ===\n");
    if (test_wiener_ns() != 0) g_fail++;
    if (test_spectral_sub() != 0) g_fail++;
    if (test_webrtc_ns() != 0) g_fail++;
    if (test_minstat_track() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_selftest();
    printf("usage: %s --selftest|--generate\n", argv[0]);
    return 2;
}
