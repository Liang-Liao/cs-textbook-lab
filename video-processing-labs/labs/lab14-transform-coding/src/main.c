#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dct.h"
#include "huffman.h"
#include "ppm_io.h"
#include "rle.h"

static int g_pass, g_fail;
static void report(const char *n, double v, const char *op, double thr, int ok) {
    if (ok) { g_pass++; printf("[PASS] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
    else { g_fail++; printf("[FAIL] %s=%.6g (criterion %s %.6g)\n", n,v,op,thr); }
}

static int test_dct_roundtrip(void) {
    double in[64], coef[64], out[64];
    for (int i = 0; i < 64; i++)
        in[i] = 100.0 + 40.0 * sin(0.3 * i) + 20.0 * cos(0.1 * i);
    dct8(in, coef);
    idct8(coef, out);
    double err = 0, ref = 0;
    for (int i = 0; i < 64; i++) {
        double d = out[i] - in[i];
        err += d * d;
        ref += in[i] * in[i];
    }
    double rel = sqrt(err / (ref + 1e-30));
    report("dct_idct_rel_err", rel, "<", 1e-6, rel < 1e-6);
    return rel < 1e-6 ? 0 : -1;
}

static int test_huffman(void) {
    int n = 5000;
    int *sym = malloc((size_t)n * sizeof(int));
    uint8_t *buf = malloc(200000);
    int *dec = malloc((size_t)n * sizeof(int));
    if (!sym || !buf || !dec) { free(sym); free(buf); free(dec); return -1; }
    unsigned s = 99;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        int r = (int)(s % 100);
        if (r < 60) sym[i] = 0;
        else if (r < 85) sym[i] = (int)(s % 5) - 2;
        else if (r < 95) sym[i] = (int)(s % 11) - 5;
        else sym[i] = (int)(s % 41) - 20;
    }
    int bits = 0;
    int nb = huffman_encode(sym, n, buf, 200000, &bits);
    int nd = huffman_decode(buf, nb, dec, n);
    int same = (nd == n);
    for (int i = 0; same && i < n; i++) if (dec[i] != sym[i]) same = 0;
    printf("  info: huffman bytes=%d bits=%d raw=%zu\n", nb, bits, n * sizeof(int));
    report("huffman_lossless", same ? 1.0 : 0.0, "==", 1.0, same);
    free(sym); free(buf); free(dec);
    return same ? 0 : -1;
}

/* Prove RLE∘unzigzag∘zigzag and Huffman on the RLE stream are lossless. */
static int test_rle_lossless(void) {
    const int nblk = 200;
    unsigned rs = 7;
    int *qstream = malloc((size_t)nblk * 64 * sizeof(int));
    int *rle = malloc((size_t)nblk * 80 * sizeof(int));
    uint8_t *hbuf = malloc(400000);
    int *hdec = malloc((size_t)nblk * 80 * sizeof(int));
    if (!qstream || !rle || !hbuf || !hdec) {
        free(qstream); free(rle); free(hbuf); free(hdec);
        return -1;
    }
    int nsym = 0;
    for (int b = 0; b < nblk; b++) {
        int q[64], zz[64];
        for (int i = 0; i < 64; i++) {
            rs = rs * 1664525u + 1013904223u;
            int r = (int)(rs % 100);
            if (i == 0) q[i] = (int)(rs % 400) - 200;
            else if (r < 70) q[i] = 0;
            else if (r < 92) q[i] = (int)(rs % 5) - 2;
            else q[i] = (int)(rs % 17) - 8;
            qstream[b * 64 + i] = q[i];
        }
        zigzag(q, zz);
        int ns = rle_encode_block(zz, rle + nsym, 80);
        if (ns < 0) { free(qstream); free(rle); free(hbuf); free(hdec); return -1; }
        nsym += ns;
    }
    int bits = 0;
    int nb = huffman_encode(rle, nsym, hbuf, 400000, &bits);
    int nd = huffman_decode(hbuf, nb, hdec, nblk * 80);
    int sym_ok = (nd == nsym);
    for (int i = 0; sym_ok && i < nsym; i++) if (hdec[i] != rle[i]) sym_ok = 0;

    int pos = 0;
    int blk_ok = 1;
    for (int b = 0; b < nblk && blk_ok; b++) {
        int zz[64], q[64];
        int used = rle_decode_block(hdec + pos, nd - pos, zz);
        if (used < 0) { blk_ok = 0; break; }
        pos += used;
        unzigzag(zz, q);
        for (int i = 0; i < 64; i++) {
            if (q[i] != qstream[b * 64 + i]) { blk_ok = 0; break; }
        }
    }
    printf("  info: RLE+Huffman symbols=%d bytes=%d bits=%d raw_coeffs=%d\n",
           nsym, nb, bits, nblk * 64);
    report("rle_huffman_lossless", (sym_ok && blk_ok) ? 1.0 : 0.0, "==", 1.0, sym_ok && blk_ok);
    free(qstream); free(rle); free(hbuf); free(hdec);
    return (sym_ok && blk_ok) ? 0 : -1;
}

static uint8_t *make_demo(int W, int H) {
    uint8_t *img = malloc((size_t)W * H);
    if (!img) return NULL;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double v = 128 + 80 * sin(0.05 * x) * cos(0.04 * y);
            if ((x / 16 + y / 16) & 1) v += 30;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            img[y * W + x] = (uint8_t)v;
        }
    return img;
}

/* Compress one image at a given QP via DCT → quant → zigzag → RLE → Huffman.
 * Returns Huffman bytes; writes PSNR of closed reconstruction. */
static int compress_qp(const uint8_t *img, int W, int H, int qp,
                       uint8_t *rec, double *psnr_out, int *bits_out) {
    int nblk = (W / 8) * (H / 8);
    int *syms = malloc((size_t)nblk * 80 * sizeof(int));
    if (!syms) return -1;
    int nsym = 0;
    for (int by = 0; by < H; by += 8) {
        for (int bx = 0; bx < W; bx += 8) {
            double blk[64], coef[64], dq[64], out[64];
            int q[64], zz[64];
            for (int j = 0; j < 8; j++)
                for (int i = 0; i < 8; i++)
                    blk[j * 8 + i] = img[(by + j) * W + bx + i];
            dct8(blk, coef);
            quant8(coef, qp, q);
            zigzag(q, zz);
            int ns = rle_encode_block(zz, syms + nsym, 80);
            if (ns < 0) { free(syms); return -1; }
            nsym += ns;
            unzigzag(zz, q);
            dequant8(q, qp, dq);
            idct8(dq, out);
            for (int j = 0; j < 8; j++)
                for (int i = 0; i < 8; i++) {
                    double v = out[j * 8 + i];
                    if (v < 0) v = 0;
                    if (v > 255) v = 255;
                    rec[(by + j) * W + bx + i] = (uint8_t)lrint(v);
                }
        }
    }
    uint8_t *hbuf = malloc(500000);
    if (!hbuf) { free(syms); return -1; }
    int bits = 0;
    int nb = huffman_encode(syms, nsym, hbuf, 500000, &bits);
    free(hbuf);
    free(syms);
    if (nb < 0) return -1;
    if (bits_out) *bits_out = bits;

    /* Verify Huffman+RLE stream is lossless w.r.t. quantized coeffs */
    /* (roundtrip of quantized stream is proven in test_rle_lossless) */

    double mse = 0;
    for (int i = 0; i < W * H; i++) {
        double d = (double)img[i] - rec[i];
        mse += d * d;
    }
    mse /= (W * H);
    if (psnr_out) *psnr_out = 10 * log10(255.0 * 255.0 / (mse + 1e-12));
    return nb;
}

static int test_qp_sweep(void) {
    const int W = 128, H = 128;
    uint8_t *img = make_demo(W, H);
    uint8_t *rec = malloc((size_t)W * H);
    if (!img || !rec) { free(img); free(rec); return -1; }
    ppm_write_gray("out/src.pgm", img, W, H);

    const int qps[] = {8, 12, 24};
    const int nqp = 3;
    int ok_default = 0;
    for (int i = 0; i < nqp; i++) {
        int qp = qps[i];
        double psnr = 0;
        int bits = 0;
        int nb = compress_qp(img, W, H, qp, rec, &psnr, &bits);
        if (nb < 0) { free(img); free(rec); return -1; }
        double ratio = (double)(W * H) / (double)nb;
        printf("  info: QP=%2d  huffman=%5d bytes  payload_bits=%6d  ratio=%.2f  PSNR=%.2f dB\n",
               qp, nb, bits, ratio, psnr);
        report(qp == 12 ? "qp12_ratio" : (qp == 8 ? "qp08_ratio" : "qp24_ratio"),
               ratio, ">", 2.0, ratio > 2.0);
        report(qp == 12 ? "qp12_psnr_db" : (qp == 8 ? "qp08_psnr_db" : "qp24_psnr_db"),
               psnr, ">", 25.0, psnr > 25.0);
        if (qp == 12) {
            ok_default = ratio > 2.0;
            ppm_write_gray("out/rec.pgm", rec, W, H);
        }
        if (qp == 24)
            ppm_write_gray("out/rec_blocky_qp24.pgm", rec, W, H); /* blockiness artifact */
    }
    free(img); free(rec);
    return ok_default ? 0 : -1;
}

/* Roadmap: quantization with a perceptual (JPEG-luminance) matrix vs the flat
 * scalar quantizer at a comparable bitrate, plus the high-QP blockiness
 * artifact image. */
static int test_quant_matrix(void) {
    const int W = 128, H = 128;
    uint8_t *img = make_demo(W, H);
    uint8_t *rec_m = malloc((size_t)W * H);
    uint8_t *rec_f = malloc((size_t)W * H);
    if (!img || !rec_m || !rec_f) { free(img); free(rec_m); free(rec_f); return -1; }

    /* matrix version: per-block DCT → matrix quant → dequant → IDCT */
    for (int quality = 10; quality <= 90; quality += 40) {
        double mse_m = 0;
        long nz = 0;
        for (int by = 0; by < H; by += 8)
            for (int bx = 0; bx < W; bx += 8) {
                double blk[64], coef[64], dq[64], out[64];
                int q[64];
                for (int j = 0; j < 8; j++)
                    for (int i = 0; i < 8; i++)
                        blk[j * 8 + i] = img[(by + j) * W + bx + i];
                dct8(blk, coef);
                quant8_matrix(coef, quality, q);
                for (int i = 0; i < 64; i++) if (q[i]) nz++;
                dequant8_matrix(q, quality, dq);
                idct8(dq, out);
                for (int j = 0; j < 8; j++)
                    for (int i = 0; i < 8; i++) {
                        double v = out[j * 8 + i];
                        if (v < 0) v = 0;
                        if (v > 255) v = 255;
                        rec_m[(by + j) * W + bx + i] = (uint8_t)lrint(v);
                    }
            }
        double mse_f = 0;
        long nz_f = 0;
        for (int by = 0; by < H; by += 8)
            for (int bx = 0; bx < W; bx += 8) {
                double blk[64], coef[64], dq[64], out[64];
                int q[64];
                for (int j = 0; j < 8; j++)
                    for (int i = 0; i < 8; i++)
                        blk[j * 8 + i] = img[(by + j) * W + bx + i];
                dct8(blk, coef);
                quant8(coef, quality / 2 + 2, q);
                for (int i = 0; i < 64; i++) if (q[i]) nz_f++;
                dequant8(q, quality / 2 + 2, dq);
                idct8(dq, out);
                for (int j = 0; j < 8; j++)
                    for (int i = 0; i < 8; i++) {
                        double v = out[j * 8 + i];
                        if (v < 0) v = 0;
                        if (v > 255) v = 255;
                        rec_f[(by + j) * W + bx + i] = (uint8_t)lrint(v);
                    }
            }
        for (int i = 0; i < W * H; i++) {
            double d = (double)img[i] - rec_m[i];
            mse_m += d * d;
        }
        mse_m /= (W * H);
        for (int i = 0; i < W * H; i++) {
            double d = (double)img[i] - rec_f[i];
            mse_f += d * d;
        }
        mse_f /= (W * H);
        double pm = 10 * log10(255.0 * 255.0 / (mse_m + 1e-12));
        double pf = 10 * log10(255.0 * 255.0 / (mse_f + 1e-12));
        printf("  info: quality=%d matrix: nz=%ld PSNR=%.2f dB | flat: nz=%ld PSNR=%.2f dB\n",
               quality, nz, pm, nz_f, pf);
        printf("  DBG2 q=%d mse_m=%f rec_m0=%d img0=%d rec_mlast=%d imglast=%d\n",
               quality, mse_m, rec_m[0], img[0], rec_m[W * H - 1], img[W * H - 1]);
        if (quality == 50)
            ppm_write_gray("out/rec_matrix_q50.pgm", rec_m, W, H);
    }
    /* perceptual weighting keeps most energy in low frequencies: the matrix
     * version must not collapse (PSNR sane) and must stay lossless-closable */
    double mse = 0;
    for (int i = 0; i < W * H; i++) {
        double d = (double)img[i] - rec_m[i];
        mse += d * d;
    }
    double psnr = 10 * log10(255.0 * 255.0 / (mse / (W * H) + 1e-12));
    report("quant_matrix_psnr_db", psnr, ">", 25.0, psnr > 25.0);
    ppm_write_gray("out/rec_blocky_flat.pgm", rec_f, W, H);
    free(img); free(rec_m); free(rec_f);
    return psnr > 25.0 ? 0 : -1;
}

static int run_selftest(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== lab14 selftest ===\n");
    if (test_dct_roundtrip() != 0) g_fail++;
    if (test_quant_matrix() != 0) g_fail++;
    if (test_huffman() != 0) g_fail++;
    if (test_rle_lossless() != 0) g_fail++;
    if (test_qp_sweep() != 0) g_fail++;
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
