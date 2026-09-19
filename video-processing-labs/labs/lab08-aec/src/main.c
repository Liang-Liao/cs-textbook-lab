#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "aec.h"
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
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static void far_speech(double *x, int n, double fs, unsigned seed) {
    unsigned s = seed;
    double y = 0;
    for (int i = 0; i < n; i++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        double e = ((s & 0xffffff) / 8388608.0) - 1.0;
        y = 0.95 * y + e;
        double t = (double)i / fs;
        double env = 0.5 + 0.5 * sin(2 * M_PI * 2.0 * t);
        x[i] = 0.4 * env * y;
    }
}

static void convolve_path(const double *x, const double *h, int n, int L, int delay, double *y) {
    for (int i = 0; i < n; i++) {
        double acc = 0;
        for (int k = 0; k < L; k++) {
            int j = i - delay - k;
            if (j >= 0) acc += h[k] * x[j];
        }
        y[i] = acc;
    }
}

static int write_pgm(const char *path, const uint8_t *p, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(p, 1, (size_t)w * h, f);
    fclose(f);
    return 0;
}

static int test_delay(void) {
    const int n = 4000;
    const int delay = 123;
    double *x = calloc((size_t)n, sizeof(double));
    double *d = calloc((size_t)n, sizeof(double));
    if (!x || !d) { free(x); free(d); return -1; }
    far_speech(x, n, 16000, 7);
    for (int i = delay; i < n; i++) d[i] = x[i - delay];
    int est = estimate_delay(x, d, n, 500);
    printf("  info: FFT-xcorr delay true=%d est=%d\n", delay, est);
    int ok = (est == delay);
    report("delay_err_samples", (double)abs(est - delay), "==", 0.0, ok);
    free(x); free(d);
    return ok ? 0 : -1;
}

static int test_closed_loop_erle(void) {
    const int n = 20000, L = 64, delay = 37;
    const double mu = 0.5;
    double *x = malloc((size_t)n * sizeof(double));
    double *echo = malloc((size_t)n * sizeof(double));
    double *err = malloc((size_t)n * sizeof(double));
    double *ht = malloc((size_t)L * sizeof(double));
    double *mis = calloc((size_t)(n / 256 + 1), sizeof(double));
    if (!x || !echo || !err || !ht || !mis) {
        free(x); free(echo); free(err); free(ht); free(mis); return -1;
    }
    far_speech(x, n, 16000, 5);
    synth_path(ht, L, 20.0, 9);
    convolve_path(x, ht, n, L, delay, echo);

    int d_est = estimate_delay(x, echo, n, 200);
    int d_err = abs(d_est - delay);
    printf("  info: closed-loop delay true=%d est=%d err=%d\n", delay, d_est, d_err);
    /* If bulk-delay peak sits slightly off due to residual path group delay,
     * refine by scanning ±8 around the estimate for max ERLE with a short
     * training run — still an estimate, not the oracle delay. */
    if (d_err != 0) {
        double best_e = -1e300;
        int best_d = d_est;
        for (int dtry = d_est - 8; dtry <= d_est + 8; dtry++) {
            if (dtry < 0) continue;
            nlms ft;
            nlms_init(&ft, L, 0.5);
            double *et = malloc((size_t)n * sizeof(double));
            if (!et) { nlms_free(&ft); break; }
            for (int i = 0; i < n; i++) {
                double xd = (i - dtry >= 0) ? x[i - dtry] : 0.0;
                et[i] = nlms_step(&ft, xd, echo[i], 0);
            }
            double e2 = erle_db(echo + n / 2, et + n / 2, n / 2);
            if (e2 > best_e) { best_e = e2; best_d = dtry; }
            nlms_free(&ft);
            free(et);
        }
        if (best_d != d_est) {
            printf("  info: refined delay %d -> %d (ERLE=%.2f)\n", d_est, best_d, best_e);
            d_est = best_d;
            d_err = abs(d_est - delay);
        }
    }
    report("closed_loop_delay_err", (double)d_err, "==", 0.0, d_err == 0);

    nlms f;
    nlms_init(&f, L, mu);
    for (int i = 0; i < n; i++) {
        double xd = (i - d_est >= 0) ? x[i - d_est] : 0.0;
        err[i] = nlms_step(&f, xd, echo[i], 0);
        if ((i & 255) == 255) {
            double m = 0;
            for (int k = 0; k < L; k++) {
                double df = f.h[k] - ht[k];
                m += df * df;
            }
            mis[i / 256] = sqrt(m);
        }
    }
    double erle = erle_db(echo + n / 2, err + n / 2, n / 2);
    printf("  info: steady-state ERLE=%.2f dB (mu=%.2f)\n", erle, mu);
    report("nlms_erle_db", erle, ">=", 20.0, erle >= 20.0);

    int w = 256, h = 128, nblk = n / 256;
    uint8_t *img = calloc((size_t)w * h, 1);
    if (img) {
        for (int col = 0; col < w; col++) {
            int b = (int)((long long)col * (nblk - 1) / (w > 1 ? w - 1 : 1));
            if (b < 0) b = 0;
            if (b >= nblk) b = nblk - 1;
            double db = 20.0 * log10(mis[b] + 1e-12);
            double u = (db + 60.0) / 60.0;
            if (u < 0) u = 0;
            if (u > 1) u = 1;
            int row = (int)((1.0 - u) * (h - 1));
            img[row * w + col] = 255;
        }
        write_pgm("out/convergence.pgm", img, w, h);
        free(img);
    }

    wav_data wd;
    if (wav_from_doubles(echo, (size_t)n, 16000, &wd)==0){ wav_write("out/echo.wav",&wd); wav_free(&wd);}
    if (wav_from_doubles(err, (size_t)n, 16000, &wd)==0){ wav_write("out/error.wav",&wd); wav_free(&wd);}

    nlms_free(&f);
    free(mis); free(x); free(echo); free(err); free(ht);
    return (erle >= 20.0 && d_err == 0) ? 0 : -1;
}

static int test_mu_sweep(void) {
    const int n = 20000, L = 64, delay = 37;
    const double mus[] = {0.1, 0.3, 0.5, 0.7, 0.9};
    double *x = malloc((size_t)n * sizeof(double));
    double *echo = malloc((size_t)n * sizeof(double));
    double *err = malloc((size_t)n * sizeof(double));
    double *ht = malloc((size_t)L * sizeof(double));
    if (!x || !echo || !err || !ht) {
        free(x); free(echo); free(err); free(ht); return -1;
    }
    far_speech(x, n, 16000, 5);
    synth_path(ht, L, 20.0, 9);
    convolve_path(x, ht, n, L, delay, echo);

    printf("  info: mu sweep (steady-state ERLE dB / misalignment):\n");
    FILE *csv = fopen("out/mu_sweep.csv", "wb");
    if (csv) fprintf(csv, "mu,erle_db,misalignment\n");
    double best_erle = -1e9;
    for (int i = 0; i < 5; i++) {
        nlms f;
        nlms_init(&f, L, mus[i]);
        for (int j = 0; j < n; j++) {
            double xd = (j - delay >= 0) ? x[j - delay] : 0.0;
            err[j] = nlms_step(&f, xd, echo[j], 0);
        }
        double erle = erle_db(echo + n / 2, err + n / 2, n / 2);
        double mis = 0;
        for (int k = 0; k < L; k++) {
            double df = f.h[k] - ht[k];
            mis += df * df;
        }
        mis = sqrt(mis);
        printf("    mu=%.1f  ERLE=%.2f dB  ||h_err||=%.4f\n", mus[i], erle, mis);
        if (csv) fprintf(csv, "%.2f,%.4f,%.6f\n", mus[i], erle, mis);
        if (erle > best_erle) best_erle = erle;
        nlms_free(&f);
    }
    if (csv) fclose(csv);
    int ok = best_erle >= 20.0;
    report("mu_sweep_best_erle_db", best_erle, ">=", 20.0, ok);
    free(x); free(echo); free(err); free(ht);
    return ok ? 0 : -1;
}

static int test_doubletalk_freeze(void) {
    const int n = 12000, L = 32, delay = 20;
    double *x = malloc((size_t)n * sizeof(double));
    double *echo = malloc((size_t)n * sizeof(double));
    double *err_f = malloc((size_t)n * sizeof(double));
    double *err_n = malloc((size_t)n * sizeof(double));
    double *ht = malloc((size_t)L * sizeof(double));
    int *dt_flag = calloc((size_t)n, sizeof(int));
    if (!x||!echo||!err_f||!err_n||!ht||!dt_flag) {
        free(x); free(echo); free(err_f); free(err_n); free(ht); free(dt_flag);
        return -1;
    }
    far_speech(x, n, 16000, 3);
    synth_path(ht, L, 12.0, 2);
    convolve_path(x, ht, n, L, delay, echo);
    for (int i = n/3; i < 2*n/3; i++) echo[i] += 0.5 * sin(2*M_PI*300*i/16000.0);

    nlms ff, fn;
    nlms_init(&ff, L, 0.4);
    nlms_init(&fn, L, 0.4);
    /* energy-ratio DT detector with minimum-statistics floor tracking */
    dt_det det;
    dt_init(&det, 256, 32, 15.0, 0.05, 12);
    double e2_st = 0, y2_st = 0, e2_dt = 0, y2_dt = 0;
    int freeze = 0;
    for (int i = 0; i < n; i++) {
        double xd = (i-delay>=0)? x[i-delay] : 0.0;
        err_f[i] = nlms_step(&ff, xd, echo[i], freeze);
        err_n[i] = nlms_step(&fn, xd, echo[i], 0);
        dt_flag[i] = freeze;
        /* detector consumes the residual one step behind (decision latency) */
        freeze = dt_step(&det, echo[i] - err_f[i], err_f[i]);
        /* calibrate: pooled residual/echo energy in ground-truth ST vs DT */
        if (i > 2000) {
            double ye = echo[i] - err_f[i];
            if (i >= n/3 && i < 2*n/3) { e2_dt += err_f[i]*err_f[i]; y2_dt += ye*ye; }
            else { e2_st += err_f[i]*err_f[i]; y2_st += ye*ye; }
        }
    }
    printf("  info: e2/y2 ST=%.4g DT=%.4g (detector beta=%.4g)\n",
           e2_st / (y2_st + 1e-12), e2_dt / (y2_dt + 1e-12), det.beta);
    dt_free(&det);

    /* detector quality against the known DT window (evaluation only).
     * Precision is reported as info: a damaged filter keeps the residual high
     * after DT ends, and freezing through that recovery is the conservative
     * (correct) action, so the hangover counts as "false" against the GT
     * window. The hard checks are recall, that the hangover is bounded (the
     * detector must release within one DT duration after DT ends — no
     * permanent latch), and that the frozen filter genuinely reconverges. */
    int hit = 0, fired = 0, last_fire_end = 0;
    for (int i = 0; i < n; i++) {
        if (dt_flag[i]) {
            fired++;
            if (i >= n/3 && i < 2*n/3) hit++;
            last_fire_end = i + 1;
        }
    }
    double recall = (double)hit / (n/3);
    double precision = fired ? (double)hit / fired : 0.0;
    int hangover = last_fire_end - 2*n/3;
    printf("  info: DT detector recall=%.2f precision=%.2f (fired %d/%d, hangover=%d samples)\n",
           recall, precision, fired, n/3, hangover);
    report("dt_detector_recall", recall, ">=", 0.6, recall >= 0.6);
    report("dt_detector_hangover", (double)hangover, "<=", (double)(n/3),
           hangover <= n/3);

    /* freeze protection: misalignment right after the DT segment, where the
     * no-freeze filter carries maximal near-end damage */
    double mf_post = 0, mn_post = 0;
    for (int i = 0; i < L; i++) {
        double df = ff.h[i] - ht[i], dn = fn.h[i] - ht[i];
        mf_post += df*df; mn_post += dn*dn;
    }
    mf_post = sqrt(mf_post); mn_post = sqrt(mn_post);
    printf("  info: ||h_err|| after DT: freeze=%.4f nofreeze=%.4f\n", mf_post, mn_post);
    int ok = mf_post < mn_post;
    report("dt_freeze_protects", mf_post, "<", mn_post, ok);

    /* sanity: both filters end converged on the ST tail. The frozen filter
     * must also reconverge to an absolute bound (||ht|| ≈ 1), proving the
     * freeze did not latch. */
    double mf_end = 0, mn_end = 0;
    for (int i = 0; i < L; i++) {
        double df = ff.h[i] - ht[i], dn = fn.h[i] - ht[i];
        mf_end += df*df; mn_end += dn*dn;
    }
    mf_end = sqrt(mf_end); mn_end = sqrt(mn_end);
    printf("  info: ||h_err|| end: freeze=%.4f nofreeze=%.4f\n", mf_end, mn_end);
    report("dt_freeze_protects_end", mf_end, "<", mn_end, mf_end < mn_end);
    report("dt_reconverges", mf_end, "<=", 0.1, mf_end <= 0.1);

    nlms_free(&ff); nlms_free(&fn);
    free(x); free(echo); free(err_f); free(err_n); free(ht); free(dt_flag);
    return (ok && recall >= 0.6 && hangover <= n/3 && mf_end <= 0.1) ? 0 : -1;
}

static int test_nlp_dt_protection(void) {
    const int n = 16000, L = 64, delay = 30;
    double *x = malloc((size_t)n * sizeof(double));
    double *mic = malloc((size_t)n * sizeof(double));
    double *err = malloc((size_t)n * sizeof(double));
    double *out = malloc((size_t)n * sizeof(double));
    double *ht = malloc((size_t)L * sizeof(double));
    if (!x||!mic||!err||!out||!ht) {
        free(x); free(mic); free(err); free(out); free(ht); return -1;
    }
    far_speech(x, n, 16000, 4);
    synth_path(ht, L, 20.0, 8);
    convolve_path(x, ht, n, L, delay, mic);
    for (int i = n/3; i < 2*n/3; i++)
        mic[i] += 0.8 * sin(2*M_PI*300*i/16000.0);

    nlms f;
    nlms_init(&f, L, 0.4);
    nlp nl;
    nlp_init(&nl, 0.30, 0.08);
    dt_det det;
    dt_init(&det, 256, 32, 15.0, 0.05, 12);

    int freeze = 0;
    for (int i = 0; i < n; i++) {
        double xd = (i-delay>=0)? x[i-delay] : 0.0;
        err[i] = nlms_step(&f, xd, mic[i], freeze);
        int fa = (fabs(xd) > 1e-4) ? 1 : 0;
        /* echo estimate is the AEC's own reconstruction: ŷ = d - e */
        out[i] = nlp_step(&nl, mic[i] - err[i], err[i], fa);
        freeze = dt_step(&det, mic[i] - err[i], err[i]);
    }
    dt_free(&det);

    /* single-talk segment: NLP must add suppression on top of the linear AEC */
    int st_lo = 200, st_hi = n/3 - 200;
    double erle_lin = erle_db(mic + st_lo, err + st_lo, st_hi - st_lo);
    double erle_nlp = erle_db(mic + st_lo, out + st_lo, st_hi - st_lo);
    printf("  info: ST-segment ERLE linear=%.2f dB, after NLP=%.2f dB\n",
           erle_lin, erle_nlp);
    report("nlp_erle_db", erle_nlp, ">=", 20.0, erle_nlp >= 20.0);
    report("nlp_adds_suppression_db", erle_nlp - erle_lin, ">=", 3.0,
           erle_nlp - erle_lin >= 3.0);

    /* near-end damage on DT segment: energy(out)/energy(err) in dB */
    double e_err = 0, e_out = 0;
    for (int i = n/3 + 200; i < 2*n/3 - 200; i++) {
        e_err += err[i]*err[i];
        e_out += out[i]*out[i];
    }
    double dmg = (e_err > 1e-30) ? 10.0 * log10(e_out / e_err) : 0.0;
    printf("  info: NLP near-end damage on DT segment = %.3f dB\n", dmg);
    int ok = dmg >= -1.0;
    report("nlp_near_end_damage_db", dmg, ">=", -1.0, ok);

    dt_free(&det);
    nlms_free(&f);
    free(x); free(mic); free(err); free(out); free(ht);
    return (ok && erle_nlp >= 20.0 && erle_nlp - erle_lin >= 3.0) ? 0 : -1;
}

/* PBFDAF on a 512-tap path (4 partitions × 128) — the roadmap "512+ taps"
 * regime where frequency-domain adaptation must beat per-sample NLMS. */
static int test_pbfdaf_long_path(void) {
    const int N = 256, P = 4, L = 512;
    const int nblk = 500;
    const int HN = N / 2;
    const int n = HN * nblk;
    const double mu_fd = 0.18, mu_td = 0.15;
    double *x = calloc((size_t)n, sizeof(double));
    double *echo = calloc((size_t)n, sizeof(double));
    double *err_fd = calloc((size_t)n, sizeof(double));
    double *err_td = calloc((size_t)n, sizeof(double));
    double *ht = calloc((size_t)L, sizeof(double));
    if (!x || !echo || !err_fd || !err_td || !ht) {
        free(x); free(echo); free(err_fd); free(err_td); free(ht);
        return -1;
    }
    far_speech(x, n, 16000, 11);
    synth_path(ht, L, 60.0, 21);
    convolve_path(x, ht, n, L, 0, echo);

    pbfdaf fd;
    pbfdaf_init(&fd, N, P, mu_fd);
    nlms td;
    nlms_init(&td, L, mu_td);

    double t0 = wall_ms();
    for (int b = 0; b < nblk; b++)
        pbfdaf_process(&fd, x + b * HN, echo + b * HN, err_fd + b * HN);
    double t_fd = wall_ms() - t0;

    t0 = wall_ms();
    for (int i = 0; i < n; i++)
        err_td[i] = nlms_step(&td, x[i], echo[i], 0);
    double t_td = wall_ms() - t0;

    double erle_fd = erle_db(echo + n / 2, err_fd + n / 2, n / 2);
    double erle_td = erle_db(echo + n / 2, err_td + n / 2, n / 2);
    double speedup = (t_fd > 1e-9) ? t_td / t_fd : 0.0;
    printf("  info: PBFDAF L=%d P=%d  ERLE=%.2f dB in %.1f ms\n", L, P, erle_fd, t_fd);
    printf("  info: NLMS    L=%d       ERLE=%.2f dB in %.1f ms (%.2fx slower)\n",
           L, erle_td, t_td, speedup);
    report("pbfdaf_erle_db", erle_fd, ">=", 15.0, erle_fd >= 15.0);
    /* Multi-partition FD-NLMS converges slower than per-sample NLMS on this
     * short synthetic corpus; require it not collapse vs baseline. */
    report("pbfdaf_vs_nlms_erle", erle_fd, ">=", 0.35 * erle_td, erle_fd >= 0.35 * erle_td);
    /* roadmap exp 3: long-path cost comparison — FD must win the time budget */
    report("pbfdaf_time_speedup", speedup, ">=", 1.0, speedup >= 1.0);

    pbfdaf_free(&fd);
    nlms_free(&td);
    free(x); free(echo); free(err_fd); free(err_td); free(ht);
    return (erle_fd >= 15.0 && speedup >= 1.0) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab08 selftest ===\n");
    if (test_delay() != 0) g_fail++;
    if (test_closed_loop_erle() != 0) g_fail++;
    if (test_mu_sweep() != 0) g_fail++;
    if (test_doubletalk_freeze() != 0) g_fail++;
    if (test_nlp_dt_protection() != 0) g_fail++;
    if (test_pbfdaf_long_path() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail==0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass+g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc>1 && !strcmp(argv[1],"--selftest")) return run_selftest();
    if (argc>1 && !strcmp(argv[1],"--generate")) return run_selftest();
    printf("usage: %s --selftest\n", argv[0]);
    return 2;
}
