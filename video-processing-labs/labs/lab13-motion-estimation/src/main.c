#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "me.h"
#include "ppm_io.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static void make_test(img *im) {
    for (int y = 0; y < im->h; y++) {
        for (int x = 0; x < im->w; x++) {
            double v = 120 + 50 * sin(0.04 * x + 0.03 * y) + 30 * cos(0.02 * x - 0.05 * y);
            if ((x - 40) * (x - 40) + (y - 30) * (y - 30) < 80) v = 220;
            if ((x - 120) * (x - 120) + (y - 90) * (y - 90) < 120) v = 30;
            if ((x - 200) * (x - 200) + (y - 150) * (y - 150) < 90) v = 180;
            /* extra high-frequency texture so SAD surface stays sharp */
            v += 6.0 * sin(0.35 * x + 0.25 * y) + 4.0 * cos(0.45 * x - 0.2 * y);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            im->p[y * im->w + x] = (unsigned char)(v + 0.5);
        }
    }
}

static int test_integer_me(void) {
    const int W = 256, H = 192, bs = 16, range = 8;
    const int tdx = 3, tdy = -2;
    img ref = img_new(W, H);
    img cur = img_new(W, H);
    make_test(&ref);
    img_shift(ref, &cur, tdx, tdy);

    int max_err = 0;
    int nblk = 0, full_ok = 0, tss_ok = 0;
    const int emx = -tdx, emy = -tdy;
    double t0 = wall_ms();
    for (int by = range; by + bs + range <= H; by += bs) {
        for (int bx = range; bx + bs + range <= W; bx += bs) {
            mv m = me_full(&ref, &cur, bx, by, bs, range);
            int ex = abs(m.x - emx), ey = abs(m.y - emy);
            if (ex > max_err) max_err = ex;
            if (ey > max_err) max_err = ey;
            nblk++;
            if (ex <= 1 && ey <= 1) full_ok++;
        }
    }
    double t_full = wall_ms() - t0;

    int max_err_tss = 0;
    t0 = wall_ms();
    for (int by = range; by + bs + range <= H; by += bs) {
        for (int bx = range; bx + bs + range <= W; bx += bs) {
            mv m = me_tss(&ref, &cur, bx, by, bs, range);
            int ex = abs(m.x - emx), ey = abs(m.y - emy);
            if (ex > max_err_tss) max_err_tss = ex;
            if (ey > max_err_tss) max_err_tss = ey;
            if (ex <= 1 && ey <= 1) tss_ok++;
        }
    }
    double t_tss = wall_ms() - t0;
    double full_rate = nblk ? (double)full_ok / nblk : 0;
    double tss_rate = nblk ? (double)tss_ok / nblk : 0;
    printf("  info: shift=(%d,%d) exp_mv=(%d,%d) full_err=%d tss_err=%d\n",
           tdx, tdy, emx, emy, max_err, max_err_tss);
    printf("  info: recovery full=%.0f%% tss=%.0f%% full=%.2fms tss=%.2fms\n",
           100 * full_rate, 100 * tss_rate, t_full, t_tss);
    report("me_full_max_err_px", (double)max_err, "<=", 1.0, max_err <= 1);
    /* TSS is a coarse search: require high block recovery, not per-block ≤1 px. */
    double tss_rate_small = nblk ? (double)tss_ok / nblk : 0;
    report("me_tss_recovery_small", tss_rate_small, ">=", 0.85, tss_rate_small >= 0.85);
    printf("  info: tss_max_err_px=%d (informational; recovery is the hard gate)\n",
           max_err_tss);

    img_free(&ref); img_free(&cur);
    return (max_err <= 1 && tss_rate_small >= 0.85) ? 0 : -1;
}

/* Larger search range + larger motion: TSS must be ≥10× faster and recover ≥90%. */
static int test_tss_large(void) {
    const int W = 320, H = 240, bs = 16, range = 15;
    const int tdx = 8, tdy = -8; /* on the TSS coarse grid */
    img ref = img_new(W, H);
    img cur = img_new(W, H);
    make_test(&ref);
    img_shift(ref, &cur, tdx, tdy);

    const int emx = -tdx, emy = -tdy;
    int nblk = 0, full_ok = 0, tss_ok = 0, diamond_ok = 0;
    double t0 = wall_ms();
    for (int by = range; by + bs + range <= H; by += bs) {
        for (int bx = range; bx + bs + range <= W; bx += bs) {
            mv m = me_full(&ref, &cur, bx, by, bs, range);
            int ex = abs(m.x - emx), ey = abs(m.y - emy);
            nblk++;
            if (ex <= 1 && ey <= 1) full_ok++;
        }
    }
    double t_full = wall_ms() - t0;

    t0 = wall_ms();
    for (int by = range; by + bs + range <= H; by += bs) {
        for (int bx = range; bx + bs + range <= W; bx += bs) {
            mv m = me_tss(&ref, &cur, bx, by, bs, range);
            int ex = abs(m.x - emx), ey = abs(m.y - emy);
            if (ex <= 1 && ey <= 1) tss_ok++;
        }
    }
    double t_tss = wall_ms() - t0;

    t0 = wall_ms();
    for (int by = range; by + bs + range <= H; by += bs) {
        for (int bx = range; bx + bs + range <= W; bx += bs) {
            mv m = me_diamond(&ref, &cur, bx, by, bs, range);
            int ex = abs(m.x - emx), ey = abs(m.y - emy);
            if (ex <= 1 && ey <= 1) diamond_ok++;
        }
    }
    double t_dia = wall_ms() - t0;

    double full_rate = nblk ? (double)full_ok / nblk : 0;
    double tss_rate = nblk ? (double)tss_ok / nblk : 0;
    double dia_rate = nblk ? (double)diamond_ok / nblk : 0;
    double speedup = t_tss > 1e-6 ? t_full / t_tss : 0.0;
    printf("  info: large ME range=%d shift=(%d,%d) nblk=%d\n", range, tdx, tdy, nblk);
    printf("  info: recovery full=%.0f%% tss=%.0f%% diamond=%.0f%%\n",
           100 * full_rate, 100 * tss_rate, 100 * dia_rate);
    printf("  info: time full=%.2fms tss=%.2fms diamond=%.2fms speedup=%.1fx\n",
           t_full, t_tss, t_dia, speedup);
    report("me_tss_speedup", speedup, ">=", 10.0, speedup >= 10.0);
    report("me_tss_recovery", tss_rate, ">=", 0.90, tss_rate >= 0.90);
    report("me_diamond_recovery", dia_rate, ">=", 0.70, dia_rate >= 0.70);

    img_free(&ref); img_free(&cur);
    return (speedup >= 10.0 && tss_rate >= 0.90 && dia_rate >= 0.70) ? 0 : -1;
}

static int test_halfpel(void) {
    const int W = 64, H = 64, bs = 16;
    img ref = img_new(W, H);
    make_test(&ref);
    /* half-pel shift by 2.5 px via bilinear resample */
    img cur = img_new(W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            double v = img_sample(&ref, x - 2.5, y - 1.5);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            cur.p[y * W + x] = (unsigned char)lrint(v);
        }
    }
    /* integer ME first then refine 0.5 */
    mv m = me_full(&ref, &cur, 16, 16, bs, 8);
    double best = 1e300;
    double bmvx = m.x, bmvy = m.y;
    for (double dy = -1.5; dy <= 1.5; dy += 0.5) {
        for (double dx = -1.5; dx <= 1.5; dx += 0.5) {
            unsigned char pred[16 * 16];
            mc_block(&ref, m.x + dx, m.y + dy, 16, 16, bs, pred);
            double s = 0;
            for (int i = 0; i < bs * bs; i++) {
                double d = pred[i] - cur.p[(16 + i / bs) * W + 16 + (i % bs)];
                s += d * d;
            }
            if (s < best) { best = s; bmvx = m.x + dx; bmvy = m.y + dy; }
        }
    }
    double errx = fabs(bmvx - (-2.5)), erry = fabs(bmvy - (-1.5));
    printf("  info: halfpel true_mv=(-2.5,-1.5) est=(%.1f,%.1f)\n", bmvx, bmvy);
    int ok = errx <= 0.51 && erry <= 0.51;
    report("halfpel_err", errx + erry, "<=", 1.02, ok);

    /* residual energy integer vs half */
    unsigned char p0[16 * 16], p1[16 * 16];
    mc_block(&ref, m.x, m.y, 16, 16, bs, p0);
    mc_block(&ref, bmvx, bmvy, 16, 16, bs, p1);
    double e0 = 0, e1 = 0;
    for (int i = 0; i < bs * bs; i++) {
        double cy = cur.p[(16 + i / bs) * W + 16 + (i % bs)];
        e0 += (p0[i] - cy) * (p0[i] - cy);
        e1 += (p1[i] - cy) * (p1[i] - cy);
    }
    printf("  info: residual int=%.0f half=%.0f\n", e0, e1);
    int ok_e = e1 <= e0 + 1e-6;
    report("halfpel_residual_better", e1, "<=", e0, ok_e);

    img_free(&ref); img_free(&cur);
    return ok ? 0 : -1;
}

/* Visualise MV field: |dx|→R, |dy|→G, energy residual→B. Also gray magnitude PGM. */
static int run_generate(void) {
    const int W = 256, H = 192, bs = 16, range = 8;
    const int tdx = 5, tdy = -3;
    img ref = img_new(W, H);
    img cur = img_new(W, H);
    make_test(&ref);
    img_shift(ref, &cur, tdx, tdy);
    ppm_write_gray("out/ref.pgm", &ref);
    ppm_write_gray("out/cur.pgm", &cur);

    int bw = W / bs, bh = H / bs;
    unsigned char *rgb = calloc((size_t)bw * bh * 3, 1);
    img mag = img_new(bw, bh);
    if (!rgb || !mag.p) { free(rgb); img_free(&mag); img_free(&ref); img_free(&cur); return -1; }

    for (int by = 0; by < bh; by++) {
        for (int bx = 0; bx < bw; bx++) {
            int px = bx * bs + range;
            int py = by * bs + range;
            if (px + bs + range > W || py + bs + range > H) continue;
            mv m = me_tss(&ref, &cur, px, py, bs, range);
            int i = (by * bw + bx) * 3;
            rgb[i + 0] = (unsigned char)(abs(m.x) * 16 > 255 ? 255 : abs(m.x) * 16);
            rgb[i + 1] = (unsigned char)(abs(m.y) * 16 > 255 ? 255 : abs(m.y) * 16);
            rgb[i + 2] = (unsigned char)((abs(m.x) + abs(m.y)) * 8 > 255 ? 255 : (abs(m.x) + abs(m.y)) * 8);
            int g = (abs(m.x) + abs(m.y)) * 12;
            if (g > 255) g = 255;
            mag.p[by * bw + bx] = (unsigned char)g;
        }
    }
    ppm_write_rgb("out/mv_field.ppm", rgb, bw, bh);
    ppm_write_gray("out/mv_field.pgm", &mag);
    printf("generated MV field out/mv_field.ppm (grid %dx%d) and refs\n", bw, bh);
    free(rgb);
    img_free(&mag);
    img_free(&ref);
    img_free(&cur);
    return 0;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab13 selftest ===\n");
    if (test_integer_me() != 0) g_fail++;
    if (test_tss_large() != 0) g_fail++;
    if (test_halfpel() != 0) g_fail++;
    /* still emit MV visualization during selftest for inspection */
    run_generate();
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail==0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass+g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc>1 && !strcmp(argv[1],"--selftest"))
        return run_selftest();
    if (argc>1 && !strcmp(argv[1],"--generate"))
        return run_generate();
    printf("usage: %s --selftest | --generate\n", argv[0]);
    return 2;
}
