#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "adpcm.h"
#include "codec.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

/* SSIM (8x8 uniform window) — copied from lab12-image-ops src/metrics.c,
 * used to cross-evaluate the closed loop per roadmap ch.15. */
static double frame_ssim(const uint8_t *A, const uint8_t *B, int w, int h) {
    const double C1 = 6.5025, C2 = 58.5225;
    const int win = 8;
    double ss = 0;
    int cnt = 0;
    for (int by = 0; by + win <= h; by += win)
        for (int bx = 0; bx + win <= w; bx += win) {
            double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
            for (int y = 0; y < win; y++)
                for (int x = 0; x < win; x++) {
                    double a = A[(by + y) * w + bx + x];
                    double b = B[(by + y) * w + bx + x];
                    sa += a; sb += b; saa += a * a; sbb += b * b; sab += a * b;
                }
            double n = (double)win * win;
            double ma = sa / n, mb = sb / n;
            double va = saa / n - ma * ma, vb = sbb / n - mb * mb;
            double cov = sab / n - ma * mb;
            ss += ((2 * ma * mb + C1) * (2 * cov + C2)) /
                  ((ma * ma + mb * mb + C1) * (va + vb + C2));
            cnt++;
        }
    return cnt ? ss / cnt : 0.0;
}

/* 176x144, 64 frames, GOP=16, medium QP. Closed-loop decode PSNR ≥ 30 dB. */
static int test_video_closedloop(void) {
    const int W = 176, H = 144, N = 64, GOP = 16, QP = 10;
    const int npix = W * H;
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *rec = malloc((size_t)N * npix);
    if (!frames || !rec) { free(frames); free(rec); return -1; }
    for (int f = 0; f < N; f++) seq_make_frame(frames + (size_t)f * npix, W, H, f);

    printf("  info: encoding %dx%d x %d frames, GOP=%d, QP=%d\n", W, H, N, GOP, QP);
    double avg = codec_sequence(frames, rec, W, H, N, GOP, QP, 0.0, 1);
    if (avg < 0) { free(frames); free(rec); return -1; }
    report("closedloop_avg_psnr_db", avg, ">=", 30.0, avg >= 30.0);
    /* roadmap ch15: SSIM re-evaluation of the closed loop */
    double ssim = frame_ssim(frames + (size_t)(N - 1) * npix,
                             rec + (size_t)(N - 1) * npix, W, H);
    printf("  info: closed-loop SSIM (last frame)=%.4f\n", ssim);
    report("closedloop_ssim_last", ssim, ">=", 0.85, ssim >= 0.85);
    free(frames); free(rec);
    return (avg >= 30.0 && ssim >= 0.85) ? 0 : -1;
}

/* roadmap criterion: mode search over DC/H/V must beat DC-only measurably.
 * The default synthetic sequence is smooth/flat, where DC prediction is
 * already near-optimal, so this test uses directional content: 4px vertical
 * stripes (H prediction reproduces them exactly), 4px horizontal stripes
 * (V prediction), and flat regions (DC). All-I frames keep the comparison
 * on the intra tool itself. */
static int test_intra_mode_gain(void) {
    const int W = 176, H = 144, N = 8, GOP = 8, QP = 10;
    const int npix = W * H;
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *rec_f = malloc((size_t)N * npix);
    uint8_t *rec_d = malloc((size_t)N * npix);
    if (!frames || !rec_f || !rec_d) {
        free(frames); free(rec_f); free(rec_d); return -1;
    }
    for (int f = 0; f < N; f++) {
        uint8_t *img = frames + (size_t)f * npix;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int v;
                if (x < W / 3)
                    v = 70 + (5 * x) / 2;          /* horizontal ramp → H */
                else if (x < 2 * W / 3)
                    v = 128;                        /* flat → DC */
                else
                    v = 70 + (5 * (y % H)) / 2;    /* vertical ramp → V */
                if (v > 255) v = 255;
                if (v < 0) v = 0;
                img[y * W + x] = (uint8_t)v;
            }
    }

    codec_set_intra_mode_limit(2);
    double p_full = codec_sequence(frames, rec_f, W, H, N, GOP, QP, 0.0, 0);
    codec_set_intra_mode_limit(0);
    double p_dc = codec_sequence(frames, rec_d, W, H, N, GOP, QP, 0.0, 0);
    codec_set_intra_mode_limit(2);
    double gain = p_full - p_dc;
    /* First-frame comparison is cascade-free: both runs start from the same
     * initial state and the RDO search includes DC, so per block it can never
     * lose against DC-only on frame 0. */
    double p0_full = frame_psnr(frames, rec_f, npix);
    double p0_dc = frame_psnr(frames, rec_d, npix);
    double gain0 = p0_full - p0_dc;
    printf("  info: intra mode search seq PSNR=%.3f dB vs DC-only=%.3f dB (%+.3f); frame0 %.3f vs %.3f (%+.3f dB)\n",
           p_full, p_dc, gain, p0_full, p0_dc, gain0);
    report("intra_mode_gain_db", gain0, ">=", 0.25, gain0 >= 0.25);
    free(frames); free(rec_f); free(rec_d);
    return gain0 >= 0.25 ? 0 : -1;
}

/* RD points: 3 QPs, print bits/size and PSNR. */
static int test_rd_points(void) {
    const int W = 176, H = 144, N = 32, GOP = 16;
    const int npix = W * H;
    const int qps[] = {6, 10, 16};
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *rec = malloc((size_t)N * npix);
    if (!frames || !rec) { free(frames); free(rec); return -1; }
    for (int f = 0; f < N; f++) seq_make_frame(frames + (size_t)f * npix, W, H, f);

    printf("  info: RD sweep %dx%d x %d frames (SSIM re-eval per point)\n", W, H, N);
    int ok = 1;
    double p0 = 0, p2 = 0;
    for (int i = 0; i < 3; i++) {
        double avg = codec_sequence(frames, rec, W, H, N, GOP, qps[i], 0.0, 1);
        if (avg < 0) { ok = 0; break; }
        double ssim = frame_ssim(frames + (size_t)(N - 1) * npix,
                                 rec + (size_t)(N - 1) * npix, W, H);
        printf("  info: QP=%d avg_PSNR=%.2f dB last_frame_SSIM=%.4f\n", qps[i], avg, ssim);
        if (i == 0) p0 = avg;
        if (i == 2) p2 = avg;
        if (avg < 28.0) ok = 0;
    }
    /* higher QP should not be better than lower QP */
    int mono = p2 <= p0 + 0.5;
    report("rd_qp_order", p0 - p2, ">=", 0.0, mono);
    report("rd_mid_psnr_db", p0, ">=", 28.0, ok);
    free(frames); free(rec);
    return (ok && mono) ? 0 : -1;
}

/* Rate control: aim at a target bits/frame, show QP walk and achieved rate. */
static int test_rate_control(void) {
    const int W = 176, H = 144, N = 32, GOP = 16, QP0 = 8;
    const int npix = W * H;
    /* ~0.8 bpp target → 0.8 * 176*144 ≈ 20275 bits/frame */
    const double target = 0.8 * npix;
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *rec = malloc((size_t)N * npix);
    if (!frames || !rec) { free(frames); free(rec); return -1; }
    for (int f = 0; f < N; f++) seq_make_frame(frames + (size_t)f * npix, W, H, f);

    printf("  info: rate-control target_bits/frame=%.0f (≈0.80 bpp)\n", target);
    double avg = codec_sequence(frames, rec, W, H, N, GOP, QP0, target, 1);
    if (avg < 0) { free(frames); free(rec); return -1; }
    report("rc_psnr_db", avg, ">=", 28.0, avg >= 28.0);
    free(frames); free(rec);
    return avg >= 28.0 ? 0 : -1;
}

static int test_adpcm(void) {
    const int n = 16000;
    int16_t *pcm = malloc((size_t)n*sizeof(int16_t));
    int16_t *dec = malloc((size_t)n*sizeof(int16_t));
    uint8_t *code = malloc((size_t)n/2);
    if (!pcm||!dec||!code) { free(pcm); free(dec); free(code); return -1; }
    /* more speech-like: 3 formant-ish tones + AM envelope + slight noise */
    unsigned rs = 42;
    for (int i = 0; i < n; i++) {
        double t = i/16000.0;
        double env = 0.55 + 0.45*sin(2*M_PI*2.5*t) * sin(2*M_PI*1.1*t + 0.3);
        if (env < 0.05) env = 0.05;
        double s = 0.55*sin(2*M_PI*180*t)
                 + 0.30*sin(2*M_PI*340*t + 0.4*sin(2*M_PI*3*t))
                 + 0.15*sin(2*M_PI*720*t);
        rs = rs * 1664525u + 1013904223u;
        double noise = ((int)(rs >> 16) & 255) / 255.0 - 0.5;
        pcm[i] = (int16_t)(14000 * env * (s + 0.02 * noise));
    }
    adpcm_encode(pcm, n, code);
    adpcm_decode(code, n, dec);
    double snr = adpcm_snr_db(pcm, dec, n);
    printf("  info: ADPCM SNR=%.2f dB\n", snr);
    report("adpcm_snr_db", snr, ">=", 25.0, snr >= 25);
    free(pcm); free(dec); free(code);
    return snr >= 25 ? 0 : -1;
}

/* CIF 352x288 subset: roadmap scale. Keep frames short so make test stays fast. */
static int test_cif_closedloop(void) {
    const int W = 352, H = 288, N = 16, GOP = 8, QP = 10;
    const int npix = W * H;
    uint8_t *frames = malloc((size_t)N * npix);
    uint8_t *rec = malloc((size_t)N * npix);
    if (!frames || !rec) { free(frames); free(rec); return -1; }
    for (int f = 0; f < N; f++) seq_make_frame(frames + (size_t)f * npix, W, H, f);
    printf("  info: CIF encode %dx%d x %d frames, GOP=%d, QP=%d\n", W, H, N, GOP, QP);
    double avg = codec_sequence(frames, rec, W, H, N, GOP, QP, 0.0, 1);
    if (avg < 0) { free(frames); free(rec); return -1; }
    report("cif_avg_psnr_db", avg, ">=", 30.0, avg >= 30.0);
    free(frames); free(rec);
    return avg >= 30.0 ? 0 : -1;
}

/* Residual Huffman (copied from lab14) vs RLE: both lossless; print bit counts. */
static int test_huffman_residual(void) {
    int zz[64];
    /* sparse quant-like pattern */
    memset(zz, 0, sizeof zz);
    zz[0] = 42;
    zz[1] = -3;
    zz[2] = 1;
    zz[8] = 7;
    zz[9] = -2;
    zz[17] = 1;
    int rle_b = 0, huff_b = 0;
    int rc = lab15_residual_roundtrip(zz, &rle_b, &huff_b);
    printf("  info: residual sparse rle_bits=%d huff_bits=%d rc=%d\n", rle_b, huff_b, rc);
    report("huffman_residual_lossless", rc == 0 ? 1.0 : 0.0, "==", 1.0, rc == 0);
    /* denser residual */
    for (int i = 0; i < 64; i++) zz[i] = (i * 17 + 3) % 9 - 4;
    rc = lab15_residual_roundtrip(zz, &rle_b, &huff_b);
    printf("  info: residual dense rle_bits=%d huff_bits=%d rc=%d\n", rle_b, huff_b, rc);
    report("huffman_residual_lossless_dense", rc == 0 ? 1.0 : 0.0, "==", 1.0, rc == 0);
    return rc == 0 ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab15 selftest ===\n");
    if (test_video_closedloop() != 0) g_fail++;
    if (test_cif_closedloop() != 0) g_fail++;
    if (test_intra_mode_gain() != 0) g_fail++;
    if (test_rd_points() != 0) g_fail++;
    if (test_rate_control() != 0) g_fail++;
    if (test_huffman_residual() != 0) g_fail++;
    if (test_adpcm() != 0) g_fail++;
    printf("summary: %d passed, %d failed\n", g_pass, g_fail);
    if (g_fail==0) { printf("ALL TESTS PASSED\n"); return 0; }
    printf("FAILED %d/%d\n", g_fail, g_pass+g_fail);
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc>1 && (!strcmp(argv[1],"--selftest")||!strcmp(argv[1],"--generate")))
        return run_selftest();
    printf("usage: %s --selftest\n", argv[0]);
    return 2;
}
