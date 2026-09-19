#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agc.h"
#include "lufs.h"
#include "wav_io.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static int test_lufs_gain(void) {
    const double fs = 48000;
    const int n = 48000;
    double *x = malloc((size_t)n * sizeof(double));
    double *y = malloc((size_t)n * sizeof(double));
    if (!x || !y) { free(x); free(y); return -1; }
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        x[i] = 0.3 * sin(2*M_PI*1000*t) + 0.2 * sin(2*M_PI*3000*t);
    }
    /* +6 dB via a scale that stays under the s16 clip boundary */
    for (int i = 0; i < n; i++) y[i] = x[i] * 1.999;
    if (peak_abs(y, n) >= 0.99) {
        /* fallback: quieter source */
        for (int i = 0; i < n; i++) x[i] *= 0.4;
        for (int i = 0; i < n; i++) y[i] = x[i] * 2.0;
    }
    double l0 = lufs_integrated(x, n, fs);
    double l1 = lufs_integrated(y, n, fs);
    double d = l1 - l0;
    printf("  info: LUFS x=%.2f y=%.2f delta=%.2f\n", l0, l1, d);
    int ok = fabs(d - 6.0) <= 0.5;
    report("lufs_plus6_delta", fabs(d - 6.0), "<=", 0.5, ok);
    free(x); free(y);
    return ok ? 0 : -1;
}

/* roadmap exp 1: sweep sine amplitudes, compare the measured input-output
 * curve against the analytic static curve (threshold/ratio/knee/makeup). */
static double gain_theory(double lvl_db, double thr_db, double ratio, double knee_db) {
    double over = lvl_db - thr_db;
    if (over <= -0.5 * knee_db) return 0.0;
    if (over >= 0.5 * knee_db) return (1.0 / ratio - 1.0) * over;
    double x = over + 0.5 * knee_db;
    return (1.0 / ratio - 1.0) * (x * x) / (2.0 * knee_db);
}

static int test_compressor_curve(void) {
    const double fs = 16000;
    compressor c = { -18.0, 4.0, 6.0, 0.0 };
    double max_err = 0;
    FILE *csv = fopen("out/compressor_curve.csv", "wb");
    if (csv) fprintf(csv, "level_db,measured_db,expected_db\n");
    for (int lvl_db = -60; lvl_db <= 0; lvl_db += 3) {
        const int n = 4000;
        double amp = pow(10.0, lvl_db / 20.0);
        double *x = malloc((size_t)n * sizeof(double));
        double *y = malloc((size_t)n * sizeof(double));
        if (!x || !y) { free(x); free(y); if (csv) fclose(csv); return -1; }
        for (int i = 0; i < n; i++)
            x[i] = amp * sin(2*M_PI*400*i/fs);
        /* caller-tracked peak envelope (instant attack) */
        double env = 0.0;
        for (int i = 0; i < n; i++) {
            double ax = fabs(x[i]);
            env = (ax > env) ? ax : env;
            y[i] = compressor_run_sample(&c, x[i], &env);
        }
        double meas_db = 20.0 * log10(peak_abs(y, n) + 1e-12);
        double exp_db = (double)lvl_db + gain_theory((double)lvl_db, c.thresh_db,
                                                     c.ratio, c.knee_db) + c.makeup_db;
        double err = fabs(meas_db - exp_db);
        if (err > max_err) max_err = err;
        if (csv) fprintf(csv, "%d,%.4f,%.4f\n", lvl_db, meas_db, exp_db);
        free(x); free(y);
    }
    if (csv) fclose(csv);
    printf("  info: compressor curve max deviation = %.3f dB over -60..0 dBFS\n", max_err);
    report("compressor_curve_max_err_db", max_err, "<=", 0.5, max_err <= 0.5);
    return (max_err <= 0.5) ? 0 : -1;
}

/* roadmap exp 3: AGC + limiter in series — large-signal input must come out
 * with zero clipped samples against the real 0 dBFS boundary. */
static int test_agc_limiter_chain(void) {
    const double fs = 16000;
    const int n = 3 * 16000;
    double *x = calloc((size_t)n, sizeof(double));
    double *y = calloc((size_t)n, sizeof(double));
    if (!x || !y) { free(x); free(y); return -1; }
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double amp = (t < 1.0) ? 0.05 : ((t < 2.0) ? 0.5 : 0.9); /* up to -1 dBFS */
        x[i] = amp * sin(2*M_PI*400*t);
    }
    agc a;
    agc_init(&a, -6.0, fs, 0.02, 0.15);
    limiter l;
    limiter_init(&l, 0.95, fs, 1.0, 50.0, 5.0);
    for (int i = 0; i < n; i++)
        y[i] = limiter_process(&l, agc_process(&a, x[i]));
    int clipped = 0;
    for (int i = 0; i < n; i++)
        if (fabs(y[i]) >= 0.999) clipped++;
    double pk = peak_abs(y, n);
    printf("  info: chain peak=%.4f (%.2f dBFS), clipped samples=%d, min limiter gain=%.3f\n",
           pk, 20.0*log10(pk+1e-12), clipped, l.gain);
    report("chain_clipped_samples", (double)clipped, "==", 0.0, clipped == 0);
    report("chain_peak_dbfs", 20.0*log10(pk+1e-12), "<", -0.1, pk < 0.989);
    limiter_free(&l);
    free(x); free(y);
    return (clipped == 0 && pk < 0.989) ? 0 : -1;
}

/* roadmap exp 2: measure overshoot for two attack/release settings — fast
 * attack bounds the post-jump overshoot, slow release avoids pumping. */
static double agc_jump_overshoot_db(double fs, double att_s, double rel_s) {
    const int n = 48000; /* 3 s, jump at t=1s */
    double *x = calloc((size_t)n, sizeof(double));
    double *y = calloc((size_t)n, sizeof(double));
    if (!x || !y) { free(x); free(y); return 1e9; }
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double amp = (t < 1.0) ? 0.05 : 0.5;
        x[i] = amp * sin(2*M_PI*400*t);
    }
    agc a;
    agc_init(&a, -6.0, fs, att_s, rel_s);
    for (int i = 0; i < n; i++) y[i] = agc_process(&a, x[i]);
    /* RMS-based overshoot over [jump, jump+150ms]: reflects the gain
     * trajectory, unlike peak which the internal safety clamp pins */
    int lo = 16000, hi = 16000 + (int)(0.15 * fs);
    double rms = rms_db(y + lo, hi - lo);
    free(x); free(y);
    return rms - a.target_db; /* dB above target right after the jump */
}

static int test_agc_step(void) {
    const double fs = 16000;
    const int n = 48000; /* 3 s */
    double *x = calloc((size_t)n, sizeof(double));
    double *y = calloc((size_t)n, sizeof(double));
    if (!x || !y) { free(x); free(y); return -1; }
    for (int i = 0; i < n; i++) {
        double t = i / fs;
        double amp = (t < 1.0) ? 0.05 : 0.5; /* +20 dB step at t=1s */
        x[i] = amp * sin(2*M_PI*400*t);
    }
    agc a;
    agc_init(&a, -6.0, fs, 0.02, 0.15);
    for (int i = 0; i < n; i++) y[i] = agc_process(&a, x[i]);

    int tail = n - (int)(0.2 * fs);
    double pk = peak_abs(y + tail, n - tail);
    double pk_db = 20.0 * log10(pk + 1e-12);
    printf("  info: tail peak=%.4f (%.2f dBFS) target=%.1f\n", pk, pk_db, a.target_db);
    int ok_conv = fabs(pk_db - a.target_db) < 2.0;
    int ok_clip = peak_abs(y, n) < 1.0;
    report("agc_settle_err_db", fabs(pk_db - a.target_db), "<", 2.0, ok_conv);
    report("agc_peak", peak_abs(y, n), "<", 1.0, ok_clip);

    /* hard-assert: converge within 1 s after level jump (t=1s → t=2s) */
    int jump_i = (int)(1.0 * fs);          /* jump at t=1s */
    int win_lo = jump_i + (int)(0.5 * fs); /* measure 0.5s after jump */
    int win_hi = jump_i + (int)(1.0 * fs); /* window ends at 1s after jump */
    if (win_hi > n) win_hi = n;
    int win_len = win_hi - win_lo;
    double pk_win = peak_abs(y + win_lo, win_len);
    double pk_win_db = 20.0 * log10(pk_win + 1e-12);
    double rms_win = rms_db(y + win_lo, win_len);
    printf("  info: post-jump window[0.5,1.0]s peak=%.2f dBFS rms=%.2f dBFS target=%.1f\n",
           pk_win_db, rms_win, a.target_db);
    int ok_jump = fabs(pk_win_db - a.target_db) <= 2.0;
    report("agc_jump_settle_1s_err_db", fabs(pk_win_db - a.target_db), "<=", 2.0, ok_jump);

    int half = n / 2;
    double *yl = malloc((size_t)half * sizeof(double));
    double *yh = malloc((size_t)half * sizeof(double));
    double *xl = malloc((size_t)half * sizeof(double));
    double *xh = malloc((size_t)half * sizeof(double));
    if (!yl || !yh || !xl || !xh) {
        free(yl); free(yh); free(xl); free(xh);
        free(x); free(y);
        return (ok_conv && ok_clip && ok_jump) ? 0 : -1;
    }
    for (int i = 0; i < half; i++) {
        double t = i / fs;
        xl[i] = 0.05 * sin(2*M_PI*400*t);
        xh[i] = 0.5 * sin(2*M_PI*400*t);
    }
    agc a2;
    agc_init(&a2, -6.0, fs, 0.02, 0.15);
    for (int i = 0; i < half; i++) yl[i] = agc_process(&a2, xl[i]);
    agc a3;
    agc_init(&a3, -6.0, fs, 0.02, 0.15);
    for (int i = 0; i < half; i++) yh[i] = agc_process(&a3, xh[i]);
    int skip = (int)(0.3 * fs);
    if (skip * 2 >= half) skip = half / 4;
    double ll = lufs_integrated(yl + skip, half - skip, (int)fs);
    double lh = lufs_integrated(yh + skip, half - skip, (int)fs);
    double dl = fabs(ll - lh);
    printf("  info: LUFS light=%.2f loud=%.2f |diff|=%.2f\n", ll, lh, dl);
    int ok_lu = dl <= 1.0;
    report("agc_lufs_diff", dl, "<=", 1.0, ok_lu);

    /* roadmap exp 2: attack/release trade-off, printed for both settings */
    double ov_fast = agc_jump_overshoot_db(fs, 0.005, 0.15);
    double ov_slow = agc_jump_overshoot_db(fs, 0.05, 0.30);
    printf("  info: overshoot after 20 dB jump: att=5ms/rel=150ms -> %.2f dB, att=50ms/rel=300ms -> %.2f dB\n",
           ov_fast, ov_slow);
    report("agc_fast_attack_overshoot_db", ov_fast, "<", 12.0,
           ov_fast < 12.0 && ov_fast < ov_slow);

    wav_data w;
    if (wav_from_doubles(y, (size_t)n, (uint32_t)fs, &w)==0){ wav_write("out/agc_out.wav",&w); wav_free(&w);}

    free(yl); free(yh); free(xl); free(xh); free(x); free(y);
    return (ok_conv && ok_clip && ok_jump && ok_lu) ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab09 selftest ===\n");
    if (test_lufs_gain() != 0) g_fail++;
    if (test_compressor_curve() != 0) g_fail++;
    if (test_agc_limiter_chain() != 0) g_fail++;
    if (test_agc_step() != 0) g_fail++;
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
