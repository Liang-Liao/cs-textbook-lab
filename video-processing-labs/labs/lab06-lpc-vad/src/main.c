#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fft.h"
#include "lpc.h"
#include "noise.h"
#include "pgm_io.h"
#include "vad.h"
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

/* ---- synthetic speech + silence with ground truth ---- */
typedef struct {
    double *x;
    int n;
    unsigned char *truth; /* per sample */
    double fs;
    /* speech intervals in samples */
    int n_seg;
    int seg_lo[8], seg_hi[8];
} corpus;

static void corpus_free(corpus *c) {
    free(c->x);
    free(c->truth);
    c->x = NULL;
    c->truth = NULL;
}

/* Build: silence / speech / silence / speech / silence.
 * Speech = all-pole vowel bursts (known formants) + mild envelope. */
static int corpus_build(corpus *c, double fs, double snr_db, unsigned seed) {
    memset(c, 0, sizeof(*c));
    c->fs = fs;
    int dur_s = 4;
    c->n = (int)(dur_s * fs);
    c->x = (double *)calloc((size_t)c->n, sizeof(double));
    c->truth = (unsigned char *)calloc((size_t)c->n, 1);
    if (!c->x || !c->truth) return -1;

    /* speech segments: [0.5,1.5] and [2.2,3.2] seconds */
    double segs[2][2] = {{0.5, 1.5}, {2.2, 3.2}};
    c->n_seg = 2;
    double f_true[3] = {730.0, 1090.0, 2440.0}; /* /a/-like */
    double bw_true[3] = {90.0, 110.0, 160.0};
    int p = 16;
    double a[17];
    lpc_from_formants(f_true, bw_true, 3, fs, a, p);

    for (int s = 0; s < 2; s++) {
        int lo = (int)lrint(segs[s][0] * fs);
        int hi = (int)lrint(segs[s][1] * fs);
        if (lo < 0) lo = 0;
        if (hi > c->n) hi = c->n;
        c->seg_lo[s] = lo;
        c->seg_hi[s] = hi;
        int len = hi - lo;
        double *v = (double *)malloc((size_t)len * sizeof(double));
        if (!v) return -1;
        lpc_synthesize_vowel(a, p, len, fs, 120.0, v);
        for (int i = 0; i < len; i++) {
            double env = 1.0;
            /* 10 ms raised-cosine edges */
            int edge = (int)(0.01 * fs);
            if (i < edge) env = 0.5 - 0.5 * cos(M_PI * (double)i / (double)edge);
            if (i >= len - edge)
                env = 0.5 - 0.5 * cos(M_PI * (double)(len - 1 - i) / (double)edge);
            c->x[lo + i] += v[i] * env;
            c->truth[lo + i] = 1;
        }
        free(v);
    }

    /* add noise at target SNR measured on speech region power vs whole */
    double ps = 0.0;
    int ns = 0;
    for (int i = 0; i < c->n; i++) {
        if (c->truth[i]) {
            ps += c->x[i] * c->x[i];
            ns++;
        }
    }
    ps = (ns > 0) ? ps / (double)ns : 1.0;
    double pn = ps / pow(10.0, snr_db / 10.0);
    double sigma = sqrt(pn);
    rng_seed(seed);
    for (int i = 0; i < c->n; i += 2) {
        double u1 = rng_uniform(), u2 = rng_uniform();
        double r = sigma * sqrt(-2.0 * log(u1));
        double th = 2.0 * M_PI * u2;
        c->x[i] += r * cos(th);
        if (i + 1 < c->n) c->x[i + 1] += r * sin(th);
    }
    return 0;
}

static void truth_to_frames(const unsigned char *sample_truth, int n, int frame,
                            int hop, unsigned char *frame_truth, int *n_frames) {
    int nf = 1 + (n - frame) / hop;
    if (nf < 1) nf = 1;
    for (int f = 0; f < nf; f++) {
        int start = f * hop;
        int votes = 0;
        for (int i = 0; i < frame; i++) {
            int idx = start + i;
            if (idx >= 0 && idx < n && sample_truth[idx]) votes++;
        }
        frame_truth[f] = (unsigned char)(votes * 2 >= frame);
    }
    *n_frames = nf;
}

/* Auto thresholds from assumed-initial silence (first 0.4 s). */
static void auto_thresholds(const double *energy_db, int n_frames, double fs, int hop,
                            double *ehi, double *elo, double *zcr_hi,
                            const double *zcr) {
    int n_noise = (int)(0.4 * fs / hop);
    if (n_noise > n_frames) n_noise = n_frames;
    if (n_noise < 1) n_noise = 1;
    double mu = 0.0, mx = -1e300, mn_z = 1e300, mx_z = -1e300;
    for (int i = 0; i < n_noise; i++) {
        mu += energy_db[i];
        if (energy_db[i] > mx) mx = energy_db[i];
        if (zcr) {
            if (zcr[i] < mn_z) mn_z = zcr[i];
            if (zcr[i] > mx_z) mx_z = zcr[i];
        }
    }
    mu /= (double)n_noise;
    double sd = 0.0;
    for (int i = 0; i < n_noise; i++) {
        double d = energy_db[i] - mu;
        sd += d * d;
    }
    sd = sqrt(sd / (double)n_noise);
    /* speech typically >> noise floor; set hi = noise + 12 dB, lo = noise + 6 dB */
    *ehi = mu + 12.0;
    *elo = mu + 6.0;
    if (sd > 0.1) {
        /* if noise is high variance, push up */
        *ehi = mu + 3.0 * sd + 6.0;
        *elo = mu + 2.0 * sd + 3.0;
    }
    /* also ensure hi is below speech energy: use global max as reference */
    double gmax = -1e300;
    for (int i = 0; i < n_frames; i++)
        if (energy_db[i] > gmax) gmax = energy_db[i];
    if (*ehi > gmax - 6.0) *ehi = mu + 0.5 * (gmax - mu);
    if (*elo > *ehi - 1.0) *elo = *ehi - 3.0;
    *zcr_hi = 0.25;
    if (zcr && mx_z > 0.0) {
        /* unused for synthetic voiced-only; keep fixed */
        (void)mn_z;
        (void)mx_z;
    }
    (void)mx;
}

static int test_vad(double snr_db, const char *tag, int hard) {
    const double fs = 16000.0;
    const int frame = 400; /* 25 ms */
    const int hop = 160;   /* 10 ms */
    corpus c;
    if (corpus_build(&c, fs, snr_db, 2024u) != 0) {
        corpus_free(&c);
        if (hard) report("vad_corpus_build", 0.0, ">=", 1.0, 0);
        return -1;
    }

    int max_frames = 1 + (c.n - frame) / hop;
    double *ed = malloc((size_t)max_frames * sizeof(double));
    double *zc = malloc((size_t)max_frames * sizeof(double));
    unsigned char *truth = malloc((size_t)max_frames);
    unsigned char *pred = malloc((size_t)max_frames);
    unsigned char *pred_gmm = malloc((size_t)max_frames);
    if (!ed || !zc || !truth || !pred || !pred_gmm) {
        if (hard) report("vad_alloc", 0.0, ">=", 1.0, 0);
        free(ed);
        free(zc);
        free(truth);
        free(pred);
        free(pred_gmm);
        corpus_free(&c);
        return -1;
    }
    int nf = 0, nfz = 0;
    frame_energy_db(c.x, c.n, frame, hop, ed, &nf);
    frame_zcr(c.x, c.n, frame, hop, zc, &nfz);
    truth_to_frames(c.truth, c.n, frame, hop, truth, &nf);

    double ehi, elo, zhi;
    auto_thresholds(ed, nf, fs, hop, &ehi, &elo, &zhi, zc);
    int hangover = 6; /* 60 ms, keep endpoint error well under 80 ms */
    vad_dual_threshold(ed, zc, nf, ehi, elo, zhi, hangover, pred);

    vad_metrics m;
    vad_eval(pred, truth, nf, hop, fs, &m);

    double endpoint = (m.start_err_ms > m.end_err_ms) ? m.start_err_ms : m.end_err_ms;
    printf("  info: [%s SNR=%.0f dB] frames=%d ehi=%.2f elo=%.2f\n", tag, snr_db, nf,
           ehi, elo);
    printf("  info: dual-threshold P=%.3f R=%.3f F1=%.3f start_err=%.1fms end_err=%.1fms\n",
           m.precision, m.recall, m.f1, m.start_err_ms, m.end_err_ms);

    char name[64];
    /* Roadmap: endpoint ≤80 ms and P/R hard gates apply at SNR≥10 dB.
       At 0 dB only the GMM-vs-threshold comparison is a pass criterion. */
    int gate_endpoints = (snr_db >= 9.5);
    int ok_ep = endpoint <= 80.0;
    int ok_p = m.precision >= 0.90;
    int ok_r = m.recall >= 0.90;
    if (hard && gate_endpoints) {
        snprintf(name, sizeof(name), "vad_endpoint_err_ms_%s", tag);
        report(name, endpoint, "<=", 80.0, ok_ep);
        snprintf(name, sizeof(name), "vad_precision_%s", tag);
        report(name, m.precision, ">=", 0.90, ok_p);
        snprintf(name, sizeof(name), "vad_recall_%s", tag);
        report(name, m.recall, ">=", 0.90, ok_r);
    } else {
        printf("  info: [%s] endpoint=%.1fms P=%.3f R=%.3f (informational)\n", tag,
               endpoint, m.precision, m.recall);
        ok_ep = ok_p = ok_r = 1;
    }

    /* GMM-EM likelihood-ratio VAD */
    gmm2 g;
    gmm2_em(ed, nf, 30, &g);
    vad_gmm(ed, nf, &g, 0.0, pred_gmm);
    vad_metrics mg;
    vad_eval(pred_gmm, truth, nf, hop, fs, &mg);
    printf("  info: GMM(mu0=%.2f var0=%.2f mu1=%.2f var1=%.2f) P=%.3f R=%.3f F1=%.3f\n",
           g.mu[0], g.var[0], g.mu[1], g.var[1], mg.precision, mg.recall, mg.f1);
    printf("  info: dual_thr F1=%.3f  GMM F1=%.3f  gap(GMM-thr)=%+.3f\n",
           m.f1, mg.f1, mg.f1 - m.f1);
    if (hard) {
        snprintf(name, sizeof(name), "vad_gmm_f1_%s", tag);
        report(name, mg.f1, ">=", 0.85, mg.f1 >= 0.85);
        /* GMM must be at least as good as threshold VAD */
        int ok_gmm_beats = (mg.f1 >= m.f1);
        snprintf(name, sizeof(name), "vad_gmm_vs_thr_f1_%s", tag);
        report(name, mg.f1 - m.f1, ">=", 0.0, ok_gmm_beats);
    }

    /* artifacts for primary SNR case */
    if (fabs(snr_db - 10.0) < 0.5) {
        wav_data w;
        if (wav_from_doubles(c.x, (size_t)c.n, (uint32_t)fs, &w) == 0) {
            wav_write("out/vad_snr10.wav", &w);
            wav_free(&w);
        }
    }

    free(ed);
    free(zc);
    free(truth);
    free(pred);
    free(pred_gmm);
    corpus_free(&c);
    if (!hard) return 0;
    int ok_gmm_hard = (mg.f1 >= 0.85) && (mg.f1 >= m.f1);
    return (ok_ep && ok_p && ok_r && ok_gmm_hard) ? 0 : -1;
}

/* ---- LPC formant test on synthetic vowel ---- */
static int test_lpc_formants(void) {
    const double fs = 16000.0;
    const int n = 800; /* 50 ms voiced frame */
    const int p = 16;
    const int nfft = 2048;
    double f_true[3] = {730.0, 1090.0, 2440.0};
    double bw_true[3] = {80.0, 100.0, 150.0};

    double a_true[17], a_est[17];
    lpc_from_formants(f_true, bw_true, 3, fs, a_true, p);
    double *x = malloc((size_t)n * sizeof(double));
    double *r = malloc((size_t)(p + 1) * sizeof(double));
    double *env = malloc((size_t)nfft * sizeof(double));
    double *f_est = malloc(8 * sizeof(double));
    if (!x || !r || !env || !f_est) {
        report("lpc_alloc", 0.0, ">=", 1.0, 0);
        free(x);
        free(r);
        free(env);
        free(f_est);
        return -1;
    }
    lpc_synthesize_vowel(a_true, p, n, fs, 120.0, x);

    /* Hamming window before autocorrelation (standard LPC practice). */
    double *xw = malloc((size_t)n * sizeof(double));
    if (!xw) {
        report("lpc_alloc", 0.0, ">=", 1.0, 0);
        free(x);
        free(r);
        free(env);
        free(f_est);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        xw[i] = x[i] * (0.54 - 0.46 * cos(2.0 * M_PI * (double)i / (double)(n - 1)));
    }
    autocorr(xw, n, p, r);
    free(xw);
    double err_e = 0.0;
    if (levinson_durbin(r, p, a_est, &err_e) != 0) {
        printf("  info: Levinson-Durbin failed\n");
        report("lpc_levinson_ok", 0.0, ">=", 1.0, 0);
        free(x);
        free(r);
        free(env);
        free(f_est);
        return -1;
    }
    printf("  info: Levinson residual energy=%.6g\n", err_e);
    lpc_envelope(a_est, p, nfft, env);
    int nf = formants_from_envelope(env, nfft, fs, 6, f_est);
    printf("  info: detected %d LPC peaks:", nf);
    for (int i = 0; i < nf; i++) printf(" %.1f", f_est[i]);
    printf(" Hz\n");
    printf("  info: true formants: %.1f %.1f %.1f Hz\n", f_true[0], f_true[1],
           f_true[2]);

    /* match each true formant to nearest detected peak; rel err */
    double worst = 0.0;
    int all_ok = (nf >= 3);
    for (int i = 0; i < 3; i++) {
        double best = 1e300, best_f = -1;
        for (int j = 0; j < nf; j++) {
            double d = fabs(f_est[j] - f_true[i]);
            if (d < best) {
                best = d;
                best_f = f_est[j];
            }
        }
        double rel = (best_f > 0) ? best / f_true[i] : 1.0;
        printf("  info: F%d true=%.1f est=%.1f rel_err=%.2f%%\n", i + 1, f_true[i],
               best_f, 100.0 * rel);
        if (rel > worst) worst = rel;
        if (rel > 0.05) all_ok = 0;
    }
    report("lpc_formant_rel_err", worst, "<=", 0.05, worst <= 0.05 && all_ok);

    /* also write STFT vs LPC envelope comparison PGM (side-by-side) */
    const int w = 512, h = 256;
    uint8_t *img = calloc((size_t)w * h, 1);
    if (img) {
        /* left half: STFT dB of frame (zero-padded FFT); right: LPC envelope */
        cpx *X = calloc((size_t)nfft, sizeof(cpx));
        double *win = malloc((size_t)n * sizeof(double));
        if (X && win) {
            for (int i = 0; i < n; i++)
                win[i] = 0.5 - 0.5 * cos(2.0 * M_PI * i / (double)n);
            for (int i = 0; i < n; i++) {
                X[i].re = x[i] * win[i];
                X[i].im = 0.0;
            }
            fft(X, nfft);
            double *stft_db = malloc((size_t)nfft * sizeof(double));
            if (stft_db) {
                double smax = -1e300, emax = -1e300;
                for (int k = 0; k < nfft / 2; k++) {
                    double m2 = X[k].re * X[k].re + X[k].im * X[k].im;
                    stft_db[k] = 10.0 * log10(m2 + 1e-20);
                    if (stft_db[k] > smax) smax = stft_db[k];
                    if (env[k] > emax) emax = env[k];
                }
                for (int col = 0; col < w / 2; col++) {
                    int k = col * (nfft / 2 - 1) / (w / 2 - 1);
                    double u = (stft_db[k] - (smax - 60.0)) / 60.0;
                    if (u < 0) u = 0;
                    if (u > 1) u = 1;
                    for (int row = 0; row < h; row++) img[row * w + col] =
                        (uint8_t)lrint(u * 255.0);
                }
                for (int col = w / 2; col < w; col++) {
                    int k = (col - w / 2) * (nfft / 2 - 1) / (w / 2 - 1);
                    double u = (env[k] - (emax - 60.0)) / 60.0;
                    if (u < 0) u = 0;
                    if (u > 1) u = 1;
                    for (int row = 0; row < h; row++) img[row * w + col] =
                        (uint8_t)lrint(u * 255.0);
                }
                free(stft_db);
            }
        }
        free(X);
        free(win);
        pgm_write("out/lpc_vs_stft.pgm", img, w, h);
        free(img);
    }

    wav_data wd;
    if (wav_from_doubles(x, (size_t)n, (uint32_t)fs, &wd) == 0) {
        wav_write("out/vowel_a.wav", &wd);
        wav_free(&wd);
    }

    free(x);
    free(r);
    free(env);
    free(f_est);
    return (worst <= 0.05 && all_ok) ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab06 selftest ===\n");
    test_vad(10.0, "snr10", 1);
    test_vad(20.0, "snr20", 1);
    /* GMM vs threshold at 0 dB — hard assert */
    test_vad(0.0, "snr0", 1);
    test_lpc_formants();
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
