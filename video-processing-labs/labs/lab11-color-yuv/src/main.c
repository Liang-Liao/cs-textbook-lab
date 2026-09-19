#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hist.h"
#include "ppm_io.h"
#include "yuv.h"

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

static double clampf(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint8_t clamp_u8(double v) {
    v = clampf(v, 0.0, 255.0);
    return (uint8_t)(v + 0.5);
}

static void make_colorbars(uint8_t *rgb, int w, int h) {
    static const uint8_t bars[8][3] = {
        {255, 255, 255}, {255, 255, 0}, {0, 255, 255}, {0, 255, 0},
        {255, 0, 255},   {255, 0, 0},   {0, 0, 255},   {0, 0, 0},
    };
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int b = col * 8 / w;
            if (b > 7) b = 7;
            int i = row * w + col;
            rgb[3 * i + 0] = bars[b][0];
            rgb[3 * i + 1] = bars[b][1];
            rgb[3 * i + 2] = bars[b][2];
        }
    }
}

static void make_low_contrast(uint8_t *rgb, int w, int h) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int i = row * w + col;
            double t = (double)col / (double)(w - 1);
            uint8_t v = (uint8_t)(60.0 + 40.0 * t);
            rgb[3 * i + 0] = v;
            rgb[3 * i + 1] = (uint8_t)(v + 8);
            rgb[3 * i + 2] = (uint8_t)(v + 16);
        }
    }
}

static double max_abs_err_rgb(const uint8_t *a, const uint8_t *b, int n) {
    double m = 0.0;
    for (int i = 0; i < n; i++) {
        for (int c = 0; c < 3; c++) {
            double d = fabs((double)a[3 * i + c] - (double)b[3 * i + c]);
            if (d > m) m = d;
        }
    }
    return m;
}

static double psnr_rgb(const uint8_t *a, const uint8_t *b, int n) {
    double mse = 0.0;
    for (int i = 0; i < n * 3; i++) {
        double d = (double)a[i] - (double)b[i];
        mse += d * d;
    }
    mse /= (double)(n * 3);
    if (mse < 1e-12) return 99.0;
    return 10.0 * log10(255.0 * 255.0 / mse);
}

static int test_roundtrip(yuv_std_t std, const char *label) {
    yuv_matrix m;
    yuv_matrix_init(&m, std);
    const int w = 64, h = 48;
    const int n = w * h;
    uint8_t *rgb = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *back = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *solid = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *solid_b = (uint8_t *)malloc((size_t)n * 3);
    double *y = (double *)malloc((size_t)n * sizeof(double));
    double *u = (double *)malloc((size_t)n * sizeof(double));
    double *v = (double *)malloc((size_t)n * sizeof(double));
    if (!rgb || !back || !solid || !solid_b || !y || !u || !v) {
        free(rgb);
        free(back);
        free(solid);
        free(solid_b);
        free(y);
        free(u);
        free(v);
        return -1;
    }
    /* dense grid of RGB triples */
    int idx = 0;
    for (int r = 0; r < 16; r++) {
        for (int g = 0; g < 16; g++) {
            for (int b = 0; b < 16 && idx < n; b++) {
                rgb[3 * idx + 0] = (uint8_t)(r * 17);
                rgb[3 * idx + 1] = (uint8_t)(g * 17);
                rgb[3 * idx + 2] = (uint8_t)(b * 17);
                idx++;
            }
        }
    }
    for (; idx < n; idx++) {
        rgb[3 * idx + 0] = (uint8_t)(idx % 256);
        rgb[3 * idx + 1] = (uint8_t)((idx * 3) % 256);
        rgb[3 * idx + 2] = (uint8_t)((idx * 7) % 256);
    }
    make_colorbars(solid, w, h);

    rgb8_to_yuv444(&m, rgb, w, h, y, u, v);
    for (int i = 0; i < n; i++) {
        y[i] = floor(y[i] + 0.5);
        u[i] = floor(u[i] + 0.5);
        v[i] = floor(v[i] + 0.5);
    }
    yuv444_to_rgb8(&m, y, u, v, w, h, back);
    double err = max_abs_err_rgb(rgb, back, n);

    rgb8_to_yuv444(&m, solid, w, h, y, u, v);
    for (int i = 0; i < n; i++) {
        y[i] = floor(y[i] + 0.5);
        u[i] = floor(u[i] + 0.5);
        v[i] = floor(v[i] + 0.5);
    }
    yuv444_to_rgb8(&m, y, u, v, w, h, solid_b);
    double err2 = max_abs_err_rgb(solid, solid_b, n);
    if (err2 > err) err = err2;

    char metric[64];
    snprintf(metric, sizeof(metric), "roundtrip_max_abs_err_%s", label);
    report(metric, err, "<=", 2.0, err <= 2.0);

    free(rgb);
    free(back);
    free(solid);
    free(solid_b);
    free(y);
    free(u);
    free(v);
    return err <= 2.0 ? 0 : -1;
}

static int test_chroma420(void) {
    yuv_matrix m;
    yuv_matrix_init(&m, YUV_STD_BT601);
    const int w = 256, h = 128;
    const int n = w * h;
    uint8_t *rgb = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *back = (uint8_t *)malloc((size_t)n * 3);
    yuv420 p;
    memset(&p, 0, sizeof(p));
    if (!rgb || !back) {
        free(rgb);
        free(back);
        return -1;
    }
    make_colorbars(rgb, w, h);
    if (rgb8_to_yuv420(&m, rgb, w, h, &p) != 0) {
        free(rgb);
        free(back);
        return -1;
    }
    yuv420_to_rgb8(&m, &p, back);
    double psnr = psnr_rgb(rgb, back, n);
    printf("  info: color-bars 4:2:0 roundtrip PSNR=%.3f dB (chroma edges lose most)\n", psnr);
    report("chroma420_edge_psnr_db", psnr, ">", 18.0, psnr > 18.0);

    /* roadmap: quantify the chroma loss ON the high-saturation color EDGES.
     * The default bars are phase-aligned with the 4:2:0 grid (lossless
     * roundtrip), so build a 3px-shifted copy: every color boundary then
     * straddles a chroma subsample, which is where 4:2:0 actually hurts. */
    uint8_t *rgb_s = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *back_s = (uint8_t *)malloc((size_t)n * 3);
    double mse_edge = 0, mse_flat = 0;
    long n_edge = 0, n_flat = 0;
    if (rgb_s && back_s) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int sx = (x + 3) % w;
                memcpy(rgb_s + 3 * (y * w + x), rgb + 3 * (y * w + sx), 3);
            }
        yuv420 ps;
        memset(&ps, 0, sizeof(ps));
        if (rgb8_to_yuv420(&m, rgb_s, w, h, &ps) == 0) {
            yuv420_to_rgb8(&m, &ps, back_s);
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    int idx = y * w + x;
                    int is_edge = 0;
                    if (x + 1 < w) {
                        int dr = abs((int)rgb_s[3 * idx] - rgb_s[3 * (idx + 1)]);
                        int dg = abs((int)rgb_s[3 * idx + 1] - rgb_s[3 * (idx + 1) + 1]);
                        int db = abs((int)rgb_s[3 * idx + 2] - rgb_s[3 * (idx + 1) + 2]);
                        if (dr + dg + db > 40) is_edge = 1;
                    }
                    if (x > 0) {
                        int dr = abs((int)rgb_s[3 * idx] - rgb_s[3 * (idx - 1)]);
                        int dg = abs((int)rgb_s[3 * idx + 1] - rgb_s[3 * (idx - 1) + 1]);
                        int db = abs((int)rgb_s[3 * idx + 2] - rgb_s[3 * (idx - 1) + 2]);
                        if (dr + dg + db > 40) is_edge = 1;
                    }
                    double d2 = 0;
                    for (int c = 0; c < 3; c++) {
                        double dd = (double)rgb_s[3 * idx + c] - back_s[3 * idx + c];
                        d2 += dd * dd;
                    }
                    if (is_edge) { mse_edge += d2; n_edge++; }
                    else { mse_flat += d2; n_flat++; }
                }
            }
            double psnr_edge = 10 * log10(3 * 255.0 * 255.0 / (mse_edge / (n_edge ? n_edge : 1) + 1e-12));
            double psnr_flat = 10 * log10(3 * 255.0 * 255.0 / (mse_flat / (n_flat ? n_flat : 1) + 1e-12));
            printf("  info: saturated-edge pixels PSNR=%.2f dB (n=%ld) vs flat-interior PSNR=%.2f dB (n=%ld)\n",
                   psnr_edge, n_edge, psnr_flat, n_flat);
            /* nearest-neighbour 4:2:0 on unaligned saturated edges loses a lot
             * (~9-10 dB here) while flat interiors stay near-lossless; the
             * roadmap asks for the number AND the concentration at edges */
            report("chroma420_saturated_edge_psnr_db", psnr_edge, ">=", 8.0,
                   psnr_edge >= 8.0);
            report("chroma420_edge_below_flat_db", psnr_flat - psnr_edge, ">=", 20.0,
                   psnr_flat - psnr_edge >= 20.0);
            ppm_write("out/colorbars_shifted_420.ppm", back_s, w, h);
        }
    }
    free(rgb_s);
    free(back_s);

    ppm_write("out/colorbars.ppm", rgb, w, h);
    ppm_write("out/colorbars_420.ppm", back, w, h);

    /* wrong: interpret YUV as RGB (Y->R, Cb->G, Cr->B), chroma upsampled */
    double *uf = (double *)malloc((size_t)n * sizeof(double));
    double *vf = (double *)malloc((size_t)n * sizeof(double));
    uint8_t *wrong = (uint8_t *)malloc((size_t)n * 3);
    if (uf && vf && wrong) {
        chroma_upsample_nearest(p.u, w / 2, h / 2, uf, w, h);
        chroma_upsample_nearest(p.v, w / 2, h / 2, vf, w, h);
        for (int i = 0; i < n; i++) {
            wrong[3 * i + 0] = clamp_u8(p.y[i]);
            wrong[3 * i + 1] = clamp_u8(uf[i]);
            wrong[3 * i + 2] = clamp_u8(vf[i]);
        }
        ppm_write("out/yuv_as_rgb_wrong.ppm", wrong, w, h);
    }
    /* wrong matrix: encode 601, decode 709 */
    yuv_matrix m709;
    yuv_matrix_init(&m709, YUV_STD_BT709);
    uint8_t *wrong_std = (uint8_t *)malloc((size_t)n * 3);
    if (wrong_std) {
        yuv420_to_rgb8(&m709, &p, wrong_std);
        ppm_write("out/decode_with_wrong_std.ppm", wrong_std, w, h);
    }
    /* Y plane as PGM */
    uint8_t *yp = (uint8_t *)malloc((size_t)n);
    if (yp) {
        for (int i = 0; i < n; i++) yp[i] = clamp_u8(p.y[i]);
        pgm_write("out/y_plane.pgm", yp, w, h);
    }

    free(uf);
    free(vf);
    free(wrong);
    free(wrong_std);
    free(yp);
    yuv420_free(&p);
    free(rgb);
    free(back);
    return psnr > 18.0 ? 0 : -1;
}

static int test_hist_eq(void) {
    yuv_matrix m;
    yuv_matrix_init(&m, YUV_STD_BT601);
    const int w = 128, h = 96;
    const int n = w * h;
    uint8_t *rgb = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *eq = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *yp = (uint8_t *)malloc((size_t)n);
    double *y = (double *)malloc((size_t)n * sizeof(double));
    double *u = (double *)malloc((size_t)n * sizeof(double));
    double *v = (double *)malloc((size_t)n * sizeof(double));
    if (!rgb || !eq || !yp || !y || !u || !v) {
        free(rgb);
        free(eq);
        free(yp);
        free(y);
        free(u);
        free(v);
        return -1;
    }
    make_low_contrast(rgb, w, h);
    rgb8_to_yuv444(&m, rgb, w, h, y, u, v);
    for (int i = 0; i < n; i++) yp[i] = clamp_u8(y[i]);

    uint8_t lo = 255, hi = 0;
    for (int i = 0; i < n; i++) {
        if (yp[i] < lo) lo = yp[i];
        if (yp[i] > hi) hi = yp[i];
    }
    int hist[256];
    hist_of_u8(yp, n, hist);
    uint8_t *himg = (uint8_t *)malloc((size_t)256 * 64);
    if (himg) {
        hist_render_pgm(hist, himg, 256, 64);
        pgm_write("out/y_hist_before.pgm", himg, 256, 64);
    }

    hist_equalize_u8(yp, n);
    uint8_t lo2 = 255, hi2 = 0;
    for (int i = 0; i < n; i++) {
        if (yp[i] < lo2) lo2 = yp[i];
        if (yp[i] > hi2) hi2 = yp[i];
    }
    hist_of_u8(yp, n, hist);
    if (himg) {
        hist_render_pgm(hist, himg, 256, 64);
        pgm_write("out/y_hist_after.pgm", himg, 256, 64);
    }
    pgm_write("out/y_eq.pgm", yp, w, h);

    /* rebuild RGB with equalized Y */
    for (int i = 0; i < n; i++) y[i] = (double)yp[i];
    yuv444_to_rgb8(&m, y, u, v, w, h, eq);
    ppm_write("out/colorbars_eq_lowcontrast.ppm", eq, w, h);

    int span_before = (int)hi - (int)lo;
    int span_after = (int)hi2 - (int)lo2;
    printf("  info: Y span before=%d (..%d), after=%d (..%d)\n", span_before, (int)hi, span_after,
           (int)hi2);
    int ok = span_after >= 200 && span_after > span_before;
    report("hist_eq_span_after", (double)span_after, ">=", 200.0, ok);

    free(himg);
    free(rgb);
    free(eq);
    free(yp);
    free(y);
    free(u);
    free(v);
    return ok ? 0 : -1;
}

static int run_selftest(void) {
    printf("=== lab11 selftest ===\n");
    if (test_roundtrip(YUV_STD_BT601, "bt601") != 0) g_fail++;
    if (test_roundtrip(YUV_STD_BT709, "bt709") != 0) g_fail++;
    if (test_chroma420() != 0) g_fail++;
    if (test_hist_eq() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("FAILED %d/%d\n", g_fail, g_pass + g_fail);
    return 1;
}

static int run_generate(void) {
    yuv_matrix m;
    yuv_matrix_init(&m, YUV_STD_BT601);
    const int w = 256, h = 128;
    const int n = w * h;
    uint8_t *rgb = (uint8_t *)malloc((size_t)n * 3);
    uint8_t *back = (uint8_t *)malloc((size_t)n * 3);
    yuv420 p;
    memset(&p, 0, sizeof(p));
    if (!rgb || !back) {
        free(rgb);
        free(back);
        return 1;
    }
    make_colorbars(rgb, w, h);
    ppm_write("out/colorbars.ppm", rgb, w, h);
    rgb8_to_yuv420(&m, rgb, w, h, &p);
    yuv420_to_rgb8(&m, &p, back);
    ppm_write("out/colorbars_420.ppm", back, w, h);
    printf("generated artifacts under out/\n");
    yuv420_free(&p);
    free(rgb);
    free(back);
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "--selftest")) return run_selftest();
    if (argc > 1 && !strcmp(argv[1], "--generate")) return run_generate();
    printf("usage: %s --selftest | --generate\n", argv[0]);
    return 2;
}
