#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codec.h"
#include "mdct.h"
#include "psycho.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static void make_music(double *sig, int n, double fs) {
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double env = 0.55 + 0.45 * sin(2 * M_PI * 2.5 * t);
        double s = 0.0;
        for (int h = 1; h <= 6; h++)
            s += (0.55 / h) * sin(2 * M_PI * 220.0 * h * t + 0.1 * h);
        s += 0.08 * sin(2 * M_PI * 1500.0 * t);
        s += 0.04 * sin(2 * M_PI * 3200.0 * t);
        sig[i] = 0.45 * env * s;
    }
}

static int test_mdct_recon(void) {
    const int N = 256;
    const int n = N * 8;
    double *sig = malloc((size_t)n * sizeof(double));
    if (!sig) return -1;
    for (int i = 0; i < n; i++) {
        double t = i / 16000.0;
        sig[i] = 0.5 * sin(2 * M_PI * 440 * t) + 0.3 * sin(2 * M_PI * 1320 * t);
    }
    double err = mdct_roundtrip_err(sig, n, N);
    printf("  info: MDCT TDAC relative err=%.3e\n", err);
    report("mdct_roundtrip_rel_err", err, "<", 1e-9, err < 1e-9);
    free(sig);
    return err < 1e-9 ? 0 : -1;
}

static int test_psycho_model(void) {
    const int n = 4096, nfft = 512;
    const double fs = 16000.0;
    double *sig = malloc((size_t)n * sizeof(double));
    double *E = calloc(PSY_MAX_BANDS, sizeof(double));
    double *thr = calloc(PSY_MAX_BANDS, sizeof(double));
    double *smr = calloc(PSY_MAX_BANDS, sizeof(double));
    bark_map bm;
    if (!sig || !E || !thr || !smr) {
        free(sig); free(E); free(thr); free(smr);
        return -1;
    }
    make_music(sig, n, fs);
    int n_bands = psycho_analyze_frame(sig + 1024, nfft, fs, E, thr, smr, &bm);
    int finite = 1, pos_thr = 0, smr_ok = 0;
    if (n_bands < 8) finite = 0;
    for (int b = 0; b < n_bands; b++) {
        if (!isfinite(thr[b]) || !isfinite(E[b]) || !isfinite(smr[b])) finite = 0;
        if (thr[b] > 0) pos_thr++;
        if (smr[b] > 1.0) smr_ok++;
    }
    double max_smr = 0.0;
    for (int b = 0; b < n_bands; b++)
        if (smr[b] > max_smr) max_smr = smr[b];
    printf("  info: psycho n_bands=%d max_smr=%.3g pos_thr=%d smr>1:%d\n", n_bands,
           max_smr, pos_thr, smr_ok);
    report("psycho_n_bands", n_bands, ">=", 16, n_bands >= 16);
    report("psycho_thr_finite_pos", pos_thr, ">=", n_bands, pos_thr == n_bands && finite);
    report("psycho_max_smr", max_smr, ">", 1.0, max_smr > 1.0 && smr_ok >= 1);
    free(sig); free(E); free(thr); free(smr);
    return (n_bands >= 16 && finite && pos_thr == n_bands && max_smr > 1.0) ? 0 : -1;
}

static int test_perceptual_codec(int generate) {
    const int N = 256;
    const double fs = 16000.0;
    const int n = (int)(fs * 0.5); /* 0.5 s */
    double *sig = malloc((size_t)n * sizeof(double));
    double *rec = calloc((size_t)n, sizeof(double));
    double *spec = NULL, *thr = NULL, *noise = NULL;
    if (!sig || !rec) {
        free(sig); free(rec);
        return -1;
    }
    make_music(sig, n, fs);
    double snr = 0, bps = 0;
    int nchk = 0, nfr = 0;
    double frac = perceptual_process(sig, n, N, fs, rec, &snr, &bps, &nchk,
                                     generate ? 1 : 0, &spec, &thr, &noise, &nfr);
    printf("  info: perceptual frac_under=%.3f snr=%.2f dB bits/sample=%.3f bands=%d\n",
           frac, snr, bps, nchk);
    report("perceptual_noise_under_thr_frac", frac, ">=", 0.70, frac >= 0.70);
    report("perceptual_snr_db", snr, ">=", 8.0, snr >= 8.0);

    double adpcm_snr = 0;
    double adpcm_frac = adpcm_noise_frac(sig, n, fs, &adpcm_snr);
    double uni_snr = 0;
    double uni_frac = uniform_noise_frac(sig, n, N, fs, 4, &uni_snr);
    printf("  info: contrast ADPCM(4bit) frac=%.3f snr=%.2f dB | uniform-MDCT4 frac=%.3f snr=%.2f dB\n",
           adpcm_frac, adpcm_snr, uni_frac, uni_snr);
    report("perceptual_beats_adpcm_frac", frac - adpcm_frac, ">", 0.0,
           frac > adpcm_frac);

    if (generate && spec && thr && noise && nfr > 0) {
        int rc = write_noise_pgm("out/noise_vs_threshold.pgm", spec, thr, noise, nfr,
                                 N / 2);
        printf("  info: wrote out/noise_vs_threshold.pgm rc=%d (%d frames x %d bins)\n",
               rc, nfr, N / 2);
        report("noise_pgm_written", rc, "==", 0, rc == 0);
    }

    free(sig); free(rec); free(spec); free(thr); free(noise);
    return frac >= 0.70 ? 0 : -1;
}

static int run_selftest(int generate) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab16 selftest ===\n");
    test_mdct_recon();
    test_psycho_model();
    test_perceptual_codec(generate);
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail==0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass+g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc>1 && !strcmp(argv[1],"--selftest"))
        return run_selftest(0);
    if (argc>1 && !strcmp(argv[1],"--generate"))
        return run_selftest(1);
    printf("usage: %s --selftest|--generate\n", argv[0]);
    return 2;
}
