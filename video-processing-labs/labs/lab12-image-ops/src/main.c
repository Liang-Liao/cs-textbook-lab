#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "conv2d.h"
#include "edge.h"
#include "metrics.h"
#include "ppm_io.h"
#include "resize.h"
#include "synth.h"

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

static uint8_t *gray_to_u8(const double *src, int n) {
    uint8_t *o = (uint8_t *)malloc((size_t)n);
    if (!o) return NULL;
    for (int i = 0; i < n; i++) {
        double v = src[i];
        if (v < 0.0) v = 0.0;
        if (v > 255.0) v = 255.0;
        o[i] = (uint8_t)(v + 0.5);
    }
    return o;
}

static void write_synth_demo(void) {
    const int w = 128, h = 128;
    uint8_t *img = (uint8_t *)malloc((size_t)w * h);
    if (!img) return;
    synth_checker(img, w, h, 8, 30, 220);
    pgm_write("out/checker.pgm", img, w, h);
    synth_circle(img, w, h, 64, 64, 40, 230, 40);
    pgm_write("out/circle.pgm", img, w, h);
    synth_gradient(img, w, h, 20, 235);
    pgm_write("out/gradient.pgm", img, w, h);
    free(img);
}

/* Roadmap exp 1: PPM/PGM readers formalized — byte-exact roundtrip plus a
 * header-validation check (bad magic must be rejected). */
static int test_io_roundtrip(void) {
    const int w = 64, h = 48;
    uint8_t *g = (uint8_t *)malloc((size_t)w * h);
    uint8_t *rgb = (uint8_t *)malloc((size_t)w * h * 3);
    uint8_t *g2 = (uint8_t *)malloc((size_t)w * h);
    uint8_t *rgb2 = (uint8_t *)malloc((size_t)w * h * 3);
    if (!g || !rgb || !g2 || !rgb2) {
        free(g); free(rgb); free(g2); free(rgb2);
        return -1;
    }
    unsigned s = 7u;
    for (int i = 0; i < w * h; i++) {
        s = s * 1103515245u + 12345u;
        g[i] = (uint8_t)((s >> 16) & 0xff);
        rgb[3 * i] = (uint8_t)((s >> 8) & 0xff);
        rgb[3 * i + 1] = (uint8_t)(s & 0xff);
        rgb[3 * i + 2] = (uint8_t)((s >> 4) & 0xff);
    }
    pgm_write("out/io_test.pgm", g, w, h);
    ppm_write("out/io_test.ppm", rgb, w, h);
    int rw = 0, rh = 0;
    int rc1 = pgm_read("out/io_test.pgm", g2, w * h, &rw, &rh);
    int ok1 = rc1 == 0 && rw == w && rh == h && memcmp(g, g2, (size_t)w * h) == 0;
    rc1 = ppm_read("out/io_test.ppm", rgb2, w * h * 3, &rw, &rh);
    int ok2 = rc1 == 0 && rw == w && rh == h && memcmp(rgb, rgb2, (size_t)w * h * 3) == 0;
    /* dimension probe without a buffer */
    int ok3 = pgm_read("out/io_test.pgm", NULL, 0, &rw, &rh) == 0 && rw == w && rh == h;
    /* header validation: truncated / wrong magic files must be rejected */
    FILE *f = fopen("out/io_bad.pgm", "wb");
    if (f) {
        fputs("P6\nnot a pgm\n", f);
        fclose(f);
    }
    int ok4 = pgm_read("out/io_bad.pgm", g2, w * h, &rw, &rh) != 0;
    int ok5 = pgm_read("out/does_not_exist.pgm", g2, w * h, &rw, &rh) != 0;
    printf("  info: pgm roundtrip=%d ppm roundtrip=%d dim probe=%d badmagic reject=%d missing reject=%d\n",
           ok1, ok2, ok3, ok4, ok5);
    report("pgm_roundtrip", ok1 ? 1.0 : 0.0, "==", 1.0, ok1);
    report("ppm_roundtrip", ok2 ? 1.0 : 0.0, "==", 1.0, ok2);
    report("pgm_reader_validation", (ok3 && ok4 && ok5) ? 1.0 : 0.0, "==", 1.0,
           ok3 && ok4 && ok5);
    free(g); free(rgb); free(g2); free(rgb2);
    return (ok1 && ok2 && ok3 && ok4 && ok5) ? 0 : -1;
}

static double now_ms(void) {
    return 1000.0 * (double)clock() / (double)CLOCKS_PER_SEC;
}

static int test_separable(void) {
    /* Larger kernel / image so wall time is measurable. */
    const int w = 256, h = 256, kn = 15;
    const int n = w * h;
    const int k2 = kn * kn;
    double *src = (double *)malloc((size_t)n * sizeof(double));
    double *d1 = (double *)malloc((size_t)n * sizeof(double));
    double *d2 = (double *)malloc((size_t)n * sizeof(double));
    double *tmp = (double *)malloc((size_t)n * sizeof(double));
    double *k1 = (double *)malloc((size_t)kn * sizeof(double));
    double *k2d = (double *)malloc((size_t)k2 * sizeof(double));
    if (!src || !d1 || !d2 || !tmp || !k1 || !k2d) {
        free(src);
        free(d1);
        free(d2);
        free(tmp);
        free(k1);
        free(k2d);
        return -1;
    }
    for (int i = 0; i < n; i++) {
        int y = i / w, x = i % w;
        src[i] = 40.0 + 180.0 * ((x + y) % 17) / 16.0 + 20.0 * sin(0.3 * x) * cos(0.2 * y);
    }
    gauss_kernel_1d(k1, kn, 3.0);
    gauss_kernel_2d(k2d, kn, 3.0);

    double t0 = now_ms();
    conv2d_direct(src, w, h, k2d, kn, kn, d1);
    double ms_direct = now_ms() - t0;

    t0 = now_ms();
    conv2d_separable(src, w, h, k1, kn, k1, tmp, d2);
    double ms_sep = now_ms() - t0;

    double err = rel_max_err(d1, d2, n);
    printf("  info: conv2d 256x256 k=%d direct=%.3f ms separable=%.3f ms speedup=%.2fx\n",
           kn, ms_direct, ms_sep, ms_sep > 1e-9 ? ms_direct / ms_sep : 0.0);
    report("separable_vs_direct_rel_err", err, "<", 1e-9, err < 1e-9);
    report("separable_ms", ms_sep, "<", ms_direct + 1e-9, ms_sep <= ms_direct + 1e-9);

    uint8_t *u1 = gray_to_u8(d1, n);
    uint8_t *u2 = gray_to_u8(d2, n);
    if (u1) pgm_write("out/conv_direct.pgm", u1, w, h);
    if (u2) pgm_write("out/conv_sep.pgm", u2, w, h);

    free(u1);
    free(u2);
    free(src);
    free(d1);
    free(d2);
    free(tmp);
    free(k1);
    free(k2d);
    return err < 1e-9 ? 0 : -1;
}

static int test_sobel(void) {
    const int w = 128, h = 128;
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    uint8_t *mag = (uint8_t *)malloc((size_t)w * h);
    if (!src || !mag) {
        free(src);
        free(mag);
        return -1;
    }
    synth_circle(src, w, h, 64, 64, 40, 230, 30);
    if (sobel_magnitude(src, w, h, mag) != 0) {
        free(src);
        free(mag);
        return -1;
    }
    /* circle edge ring should be much brighter than interior */
    double edge_sum = 0.0, mid_sum = 0.0;
    int edge_n = 0, mid_n = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double dx = x - 64, dy = y - 64;
            double r = sqrt(dx * dx + dy * dy);
            int v = mag[y * w + x];
            if (r > 36.0 && r < 44.0) {
                edge_sum += v;
                edge_n++;
            } else if (r < 20.0) {
                mid_sum += v;
                mid_n++;
            }
        }
    }
    double edge_m = (edge_n > 0) ? edge_sum / edge_n : 0.0;
    double mid_m = (mid_n > 0) ? mid_sum / mid_n : 0.0;
    int ok = edge_m > 40.0 && edge_m > mid_m * 4.0;
    printf("  info: sobel edge mean=%.2f, interior mean=%.2f\n", edge_m, mid_m);
    report("sobel_edge_boost", edge_m, ">", 40.0, ok);
    pgm_write("out/sobel.pgm", mag, w, h);
    free(src);
    free(mag);
    return ok ? 0 : -1;
}

static int test_bilinear(void) {
    const int w = 32, h = 24;
    const int dw = w * 2, dh = h * 2;
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    uint8_t *dst = (uint8_t *)malloc((size_t)dw * dh);
    uint8_t *nn = (uint8_t *)malloc((size_t)dw * dh);
    if (!src || !dst || !nn) {
        free(src);
        free(dst);
        free(nn);
        return -1;
    }
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            src[y * w + x] = (uint8_t)((x * 7 + y * 13) & 255);
        }
    }
    if (bilinear_resize2x(src, w, h, dst) != 0 || nearest_resize2x(src, w, h, nn) != 0) {
        free(src);
        free(dst);
        free(nn);
        return -1;
    }
    int maxd = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int a = src[y * w + x];
            int b = dst[(2 * y) * dw + (2 * x)];
            int d = a > b ? a - b : b - a;
            if (d > maxd) maxd = d;
        }
    }
    report("bilinear_intgrid_max_abs", (double)maxd, "<=", 0.0, maxd <= 0);
    /* nearest also hits integer grid exactly by construction */
    int nn_ok = 1;
    for (int y = 0; y < h && nn_ok; y++) {
        for (int x = 0; x < w; x++) {
            if (nn[(2 * y) * dw + (2 * x)] != src[y * w + x]) {
                nn_ok = 0;
                break;
            }
        }
    }
    report("nearest_intgrid_exact", nn_ok ? 1.0 : 0.0, "==", 1.0, nn_ok);
    pgm_write("out/resize2x.pgm", dst, dw, dh);
    pgm_write("out/resize2x_nn.pgm", nn, dw, dh);
    free(src);
    free(dst);
    free(nn);
    return (maxd <= 0 && nn_ok) ? 0 : -1;
}

static int test_ssim_gain(void) {
    const int w = 128, h = 128;
    const int n = w * h;
    uint8_t *ref = (uint8_t *)malloc((size_t)n);
    uint8_t *gain = (uint8_t *)malloc((size_t)n);
    uint8_t *noise = (uint8_t *)malloc((size_t)n);
    if (!ref || !gain || !noise) {
        free(ref);
        free(gain);
        free(noise);
        return -1;
    }
    synth_ssim_ref(ref, w, h);
    pure_gain(ref, gain, n, 0.92);
    /* match-ish PSNR with additive white noise for comparison */
    unsigned int s = 12345u;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        int r = (int)((s >> 16) & 255) - 128;
        int v = (int)ref[i] + r / 4; /* ~±32 */
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        noise[i] = (uint8_t)v;
    }
    double pg = psnr_u8(ref, gain, n);
    double sg = ssim_u8(ref, gain, w, h, 8);
    double pn = psnr_u8(ref, noise, n);
    double sn = ssim_u8(ref, noise, w, h, 8);
    printf("  info: pure-gain  PSNR=%.3f dB  SSIM=%.4f\n", pg, sg);
    printf("  info: white-noise PSNR=%.3f dB  SSIM=%.4f\n", pn, sn);
    /* SSIM less sensitive to pure gain: stays high even when PSNR drops */
    int ok_ssim = sg >= 0.85;
    int ok_psnr_drops = pg < 40.0;
    report("ssim_pure_gain", sg, ">=", 0.85, ok_ssim);
    report("psnr_pure_gain_db", pg, "<", 40.0, ok_psnr_drops);

    pgm_write("out/ref.pgm", ref, w, h);
    pgm_write("out/gain092.pgm", gain, w, h);
    pgm_write("out/noise.pgm", noise, w, h);
    free(ref);
    free(gain);
    free(noise);
    return (ok_ssim && ok_psnr_drops) ? 0 : -1;
}

static int test_ssim_vs_psnr_rank(void) {
    const int w = 128, h = 128;
    const int n = w * h;
    uint8_t *ref = (uint8_t *)malloc((size_t)n);
    uint8_t *bright = (uint8_t *)malloc((size_t)n);
    uint8_t *warp = (uint8_t *)malloc((size_t)n);
    if (!ref || !bright || !warp) {
        free(ref);
        free(bright);
        free(warp);
        return -1;
    }
    synth_ssim_ref(ref, w, h);
    brightness_shift(ref, bright, n, 30);
    comb_warp_h(ref, warp, w, h, 1);

    double pb = psnr_u8(ref, bright, n);
    double pw = psnr_u8(ref, warp, n);
    double sb = ssim_u8(ref, bright, w, h, 8);
    double sw = ssim_u8(ref, warp, w, h, 8);
    printf("  info: brightness+30  PSNR=%.3f dB  SSIM=%.4f\n", pb, sb);
    printf("  info: comb-warp 1px  PSNR=%.3f dB  SSIM=%.4f\n", pw, sw);

    int ok_psnr = pw > pb;
    int ok_ssim = sb > sw;
    report("rank_psnr_prefers_warp", pw - pb, ">", 0.0, ok_psnr);
    report("rank_ssim_prefers_bright", sb - sw, ">", 0.0, ok_ssim);

    pgm_write("out/bright.pgm", bright, w, h);
    pgm_write("out/warp.pgm", warp, w, h);
    free(ref);
    free(bright);
    free(warp);
    return (ok_psnr && ok_ssim) ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab12 selftest ===\n");
    write_synth_demo();
    if (test_io_roundtrip() != 0) g_fail++;
    if (test_separable() != 0) g_fail++;
    if (test_sobel() != 0) g_fail++;
    if (test_bilinear() != 0) g_fail++;
    if (test_ssim_gain() != 0) g_fail++;
    if (test_ssim_vs_psnr_rank() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    write_synth_demo();
    const int w = 128, h = 128;
    uint8_t *src = (uint8_t *)malloc((size_t)w * h);
    uint8_t *mag = (uint8_t *)malloc((size_t)w * h);
    if (src && mag) {
        synth_circle(src, w, h, 64, 64, 40, 230, 30);
        sobel_magnitude(src, w, h, mag);
        pgm_write("out/sobel.pgm", mag, w, h);
    }
    free(src);
    free(mag);
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
