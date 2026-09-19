#include "codec.h"
#include "huffman.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- DCT ---------- */

void dct8(const double in[64], double out[64]) {
    for (int u = 0; u < 8; u++) for (int v = 0; v < 8; v++) {
        double s = 0;
        for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++)
            s += in[y*8+x]*cos((2*x+1)*u*M_PI/16)*cos((2*y+1)*v*M_PI/16);
        double cu = u?1:sqrt(0.5), cv = v?1:sqrt(0.5);
        out[v*8+u] = 0.25*cu*cv*s;
    }
}

void idct8(const double in[64], double out[64]) {
    for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++) {
        double s = 0;
        for (int u = 0; u < 8; u++) for (int v = 0; v < 8; v++) {
            double cu = u?1:sqrt(0.5), cv = v?1:sqrt(0.5);
            s += cu*cv*in[v*8+u]*cos((2*x+1)*u*M_PI/16)*cos((2*y+1)*v*M_PI/16);
        }
        out[y*8+x] = 0.25*s;
    }
}

static const int ZZ[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

/* ---------- bitstream ---------- */

void bs_init(bitstream *bs, uint8_t *buf, int cap_bytes) {
    bs->buf = buf;
    bs->cap = cap_bytes;
    bs->bitpos = 0;
    if (buf && cap_bytes > 0) memset(buf, 0, (size_t)cap_bytes);
}

void bs_open(bitstream *bs, uint8_t *buf, int cap_bytes) {
    bs->buf = buf;
    bs->cap = cap_bytes;
    bs->bitpos = 0;
}

void bs_put(bitstream *bs, unsigned v, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        int bp = bs->bitpos++;
        if (bs->buf && (bp >> 3) < bs->cap) {
            if ((v >> i) & 1u)
                bs->buf[bp >> 3] |= (uint8_t)(1u << (7 - (bp & 7)));
        }
    }
}

void bs_put_se(bitstream *bs, int v) {
    unsigned u = (v <= 0) ? (unsigned)(-2 * v) : (unsigned)(2 * v - 1);
    unsigned x = u + 1u;
    int n = 0;
    unsigned t = x;
    while (t) { n++; t >>= 1; }
    bs_put(bs, 0, n - 1);
    bs_put(bs, x, n);
}

int bs_get(bitstream *bs, int nbits) {
    unsigned v = 0;
    for (int i = 0; i < nbits; i++) {
        int bp = bs->bitpos++;
        if (!bs->buf || (bp >> 3) >= bs->cap) return -1;
        v = (v << 1) | ((bs->buf[bp >> 3] >> (7 - (bp & 7))) & 1u);
    }
    return (int)v;
}

int bs_get_se(bitstream *bs) {
    int z = 0;
    for (;;) {
        int b = bs_get(bs, 1);
        if (b < 0) return 0x7fffffff;
        if (b == 1) break;
        z++;
        if (z > 30) return 0x7fffffff;
    }
    unsigned info = 0;
    if (z > 0) {
        int rest = bs_get(bs, z);
        if (rest < 0) return 0x7fffffff;
        info = (unsigned)rest;
    }
    unsigned x = (1u << z) | info;
    unsigned u = x - 1u;
    if (u & 1u) return (int)((u + 1u) / 2u);
    return -(int)(u / 2u);
}

int bs_bytes(const bitstream *bs) {
    return (bs->bitpos + 7) / 8;
}

/* ---------- synthetic sequence ---------- */

void seq_make_frame(uint8_t *img, int w, int h, int t) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double v = 110.0
                + 50.0 * sin(0.07 * x + 0.05 * t) * cos(0.06 * y)
                + 25.0 * cos(0.03 * (x + y) + 0.08 * t)
                + 10.0 * sin(0.4 * x + 0.3 * y);
            int span = w > 80 ? w - 80 : 1;
            int rx = 30 + (t * 2) % span;
            int ry = 40;
            if (x >= rx && x < rx + 36 && y >= ry && y < ry + 28) v = 220;
            int cx = w - 40 - (t % 60);
            int cy = h / 2 + ((t * 3) % 40) - 20;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy < 180) v = 35;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            img[y * w + x] = (uint8_t)(v + 0.5);
        }
    }
}

double frame_psnr(const uint8_t *a, const uint8_t *b, int n) {
    double mse = 0;
    for (int i = 0; i < n; i++) {
        double d = (double)a[i] - b[i];
        mse += d * d;
    }
    mse /= (double)n;
    return 10.0 * log10(255.0 * 255.0 / (mse + 1e-12));
}

/* ---------- residual quant + RLE ---------- */

static void quant_zz(const double coef[64], int qp, int zz[64]) {
    int q[64];
    if (qp < 1) qp = 1;
    for (int i = 0; i < 64; i++) q[i] = (int)lround(coef[i] / qp);
    for (int i = 0; i < 64; i++) zz[i] = q[ZZ[i]];
}

static void dequant_unzz(const int zz[64], int qp, double coef[64]) {
    int q[64];
    if (qp < 1) qp = 1;
    for (int i = 0; i < 64; i++) q[ZZ[i]] = zz[i];
    for (int i = 0; i < 64; i++) coef[i] = q[i] * qp;
}

static void put_residual_rle(bitstream *bs, const int zz[64]) {
    bs_put_se(bs, zz[0]);
    int i = 1;
    while (i < 64) {
        int run = 0;
        while (i < 64 && zz[i] == 0) { run++; i++; }
        if (i >= 64) break;
        bs_put(bs, 0u, 1);
        bs_put(bs, (unsigned)run, 6);
        bs_put_se(bs, zz[i]);
        i++;
    }
    bs_put(bs, 1u, 1);
}

static int get_residual_rle(bitstream *bs, int zz[64]) {
    for (int k = 0; k < 64; k++) zz[k] = 0;
    int dc = bs_get_se(bs);
    if (dc == 0x7fffffff) return -1;
    zz[0] = dc;
    int i = 1;
    for (;;) {
        int eob = bs_get(bs, 1);
        if (eob < 0) return -1;
        if (eob) break;
        int run = bs_get(bs, 6);
        if (run < 0) return -1;
        int level = bs_get_se(bs);
        if (level == 0x7fffffff) return -1;
        i += run;
        if (i >= 64) return -1;
        zz[i++] = level;
    }
    return 0;
}

/* Primary residual path: Huffman on the 64 zigzag levels (lab14). */
static void put_residual(bitstream *bs, const int zz[64]) {
    uint8_t hbuf[512];
    int hbits = 0;
    int nbytes = huffman_encode(zz, 64, hbuf, (int)sizeof hbuf, &hbits);
    if (nbytes < 0) { put_residual_rle(bs, zz); return; }
    bs_put(bs, 0u, 1); /* tag: huffman */
    bs_put(bs, (unsigned)nbytes, 16);
    for (int i = 0; i < nbytes; i++) bs_put(bs, hbuf[i], 8);
}

static int get_residual(bitstream *bs, int zz[64]) {
    int tag = bs_get(bs, 1);
    if (tag < 0) return -1;
    if (tag == 1) {
        /* legacy/rle tag unused in encode; keep decoder simple */
        return -1;
    }
    int nbytes = bs_get(bs, 16);
    if (nbytes < 0 || nbytes > 512) return -1;
    uint8_t hbuf[512];
    for (int i = 0; i < nbytes; i++) {
        int b = bs_get(bs, 8);
        if (b < 0) return -1;
        hbuf[i] = (uint8_t)b;
    }
    int n = huffman_decode(hbuf, nbytes, zz, 64);
    return (n == 64) ? 0 : -1;
}

/* Exposed for selftest: lossless RLE vs Huffman on the same zz. */
int lab15_residual_roundtrip(const int zz_in[64], int *out_rle_bits, int *out_huff_bits) {
    uint8_t buf[1024];
    int zz_out[64];
    bitstream bs;
    bs_init(&bs, buf, (int)sizeof buf);
    put_residual_rle(&bs, zz_in);
    int rle_bits = bs.bitpos;
    bs_open(&bs, buf, (int)sizeof buf);
    if (get_residual_rle(&bs, zz_out) != 0) return -1;
    for (int i = 0; i < 64; i++) if (zz_out[i] != zz_in[i]) return -2;

    bs_init(&bs, buf, (int)sizeof buf);
    put_residual(&bs, zz_in);
    int huff_bits = bs.bitpos;
    bs_open(&bs, buf, (int)sizeof buf);
    if (get_residual(&bs, zz_out) != 0) return -3;
    for (int i = 0; i < 64; i++) if (zz_out[i] != zz_in[i]) return -4;
    if (out_rle_bits) *out_rle_bits = rle_bits;
    if (out_huff_bits) *out_huff_bits = huff_bits;
    return 0;
}

/* ---------- I frame ---------- */

/* Intra mode decision limit: 0 = DC only (ablation), 2 = full search. */
static int g_intra_max_mode = 2;
void codec_set_intra_mode_limit(int max_mode) {
    if (max_mode < 0) max_mode = 0;
    if (max_mode > 2) max_mode = 2;
    g_intra_max_mode = max_mode;
}

static void make_intra_pred(const uint8_t *rec, int stride, int bx, int by,
                            int mode, uint8_t pred[64]);

/* True mode search with a rate-distortion-style decision: for every candidate
 * the residual goes through the full quantize/dequantize/reconstruct path and
 * the mode with the smallest actual reconstruction SSD wins. (Plain SAD on
 * the source block is only a proxy and can disagree with the post-quantization
 * distortion.) Predictors come from the reconstructed neighborhood (dst), so
 * encoder and decoder see identical predictions. */
static int pick_intra_mode(const uint8_t *rec, int stride, int bx, int by,
                           const uint8_t *src, int qp) {
    double best_ssd = 1e300;
    int best_mode = 0;
    for (int mode = 0; mode <= g_intra_max_mode; mode++) {
        uint8_t pred[64];
        make_intra_pred(rec, stride, bx, by, mode, pred);
        double res[64], coef[64], dq[64], recd[64];
        for (int i = 0; i < 64; i++)
            res[i] = (double)src[i] - pred[i];
        dct8(res, coef);
        int zz[64];
        quant_zz(coef, qp, zz);
        dequant_unzz(zz, qp, dq);
        idct8(dq, recd);
        double ssd = 0.0;
        for (int i = 0; i < 64; i++) {
            double v = pred[i] + recd[i];
            double d = v - (double)src[i];
            ssd += d * d;
        }
        if (ssd < best_ssd) {
            best_ssd = ssd;
            best_mode = mode;
        }
    }
    return best_mode;
}

static void make_intra_pred(const uint8_t *rec, int stride, int bx, int by,
                            int mode, uint8_t pred[64]) {
    /* Use reconstructed left/above neighbors so encoder and decoder match.
     * Outside the frame the neighbor is 128. */
    uint8_t left[8], above[8];
    for (int y = 0; y < 8; y++) left[y] = (bx > 0) ? rec[(by + y) * stride + (bx - 1)] : 128;
    for (int x = 0; x < 8; x++) above[x] = (by > 0) ? rec[(by - 1) * stride + (bx + x)] : 128;
    if (mode == 0) {
        double s = 0;
        for (int y = 0; y < 8; y++) s += left[y];
        for (int x = 0; x < 8; x++) s += above[x];
        uint8_t dc = (uint8_t)(s / 16.0 + 0.5);
        for (int i = 0; i < 64; i++) pred[i] = dc;
    } else if (mode == 1) {
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) pred[y * 8 + x] = left[y];
    } else {
        for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) pred[y * 8 + x] = above[x];
    }
}

static void recon_block(const uint8_t pred[64], const double rec[64],
                        uint8_t *dst, int ostride) {
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        double v = pred[y * 8 + x] + rec[y * 8 + x];
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        dst[y * ostride + x] = (uint8_t)lrint(v);
    }
}

int frame_i_encode(const uint8_t *src, uint8_t *dst, int w, int h, int qp,
                   bitstream *bs) {
    if (qp < 1) qp = 1;
    int start = bs->bitpos;
    memset(dst, 128, (size_t)w * (size_t)h);
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            const uint8_t *sb = src + by * w + bx;
            int mode = pick_intra_mode(dst, w, bx, by, sb, qp);
            bs_put(bs, (unsigned)mode, 2);
            uint8_t pred[64];
            make_intra_pred(dst, w, bx, by, mode, pred);
            double res[64], coef[64], dq[64], rec[64];
            for (int i = 0; i < 64; i++)
                res[i] = (double)sb[(i / 8) * w + (i % 8)] - pred[i];
            dct8(res, coef);
            int zz[64];
            quant_zz(coef, qp, zz);
            put_residual(bs, zz);
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return bs->bitpos - start;
}

int frame_i_decode(uint8_t *dst, int w, int h, int qp, bitstream *bs) {
    if (qp < 1) qp = 1;
    memset(dst, 128, (size_t)w * (size_t)h);
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            int mode = bs_get(bs, 2);
            if (mode < 0) return -1;
            uint8_t pred[64];
            make_intra_pred(dst, w, bx, by, mode, pred);
            int zz[64];
            if (get_residual(bs, zz) != 0) return -1;
            double dq[64], rec[64];
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return 0;
}

/* ---------- P frame ---------- */

typedef struct { int x, y; } mv;

static double blk_sad(const uint8_t *a, const uint8_t *b, int sa, int sb) {
    double s = 0;
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
        s += fabs((double)a[y * sa + x] - (double)b[y * sb + x]);
    return s;
}

static mv me_block(const uint8_t *ref, const uint8_t *cur, int w, int h,
                   int bx, int by, int range) {
    mv best = {0, 0};
    double bv = 1e300;
    for (int my = -range; my <= range; my++) {
        for (int mx = -range; mx <= range; mx++) {
            int rx = bx + mx, ry = by + my;
            if (rx < 0 || ry < 0 || rx + 8 > w || ry + 8 > h) continue;
            double s = blk_sad(cur + by * w + bx, ref + ry * w + rx, w, w);
            if (s < bv) { bv = s; best.x = mx; best.y = my; }
        }
    }
    return best;
}

static void mc_pred(const uint8_t *ref, int w, int h, int bx, int by,
                    mv m, uint8_t pred[64]) {
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        int sx = bx + x + m.x, sy = by + y + m.y;
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= w) sx = w - 1;
        if (sy >= h) sy = h - 1;
        pred[y * 8 + x] = ref[sy * w + sx];
    }
}

int frame_p_encode(const uint8_t *ref, const uint8_t *src, uint8_t *dst,
                   int w, int h, int qp, bitstream *bs) {
    if (qp < 1) qp = 1;
    const int range = 8;
    int start = bs->bitpos;
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            mv best = me_block(ref, src, w, h, bx, by, range);
            bs_put_se(bs, best.x);
            bs_put_se(bs, best.y);
            uint8_t pred[64];
            mc_pred(ref, w, h, bx, by, best, pred);
            double res[64], coef[64], dq[64], rec[64];
            for (int i = 0; i < 64; i++)
                res[i] = (double)src[(by + i / 8) * w + bx + i % 8] - pred[i];
            dct8(res, coef);
            int zz[64];
            quant_zz(coef, qp, zz);
            put_residual(bs, zz);
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return bs->bitpos - start;
}

int frame_p_decode(const uint8_t *ref, uint8_t *dst, int w, int h, int qp,
                   bitstream *bs) {
    if (qp < 1) qp = 1;
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            int mvx = bs_get_se(bs);
            int mvy = bs_get_se(bs);
            if (mvx == 0x7fffffff || mvy == 0x7fffffff) return -1;
            mv m = {mvx, mvy};
            uint8_t pred[64];
            mc_pred(ref, w, h, bx, by, m, pred);
            int zz[64];
            if (get_residual(bs, zz) != 0) return -1;
            double dq[64], rec[64];
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return 0;
}

/* ---------- full sequence: encode, rate-control, closed-loop decode ---------- */

double codec_sequence(const uint8_t *frames, uint8_t *rec, int w, int h, int n,
                      int gop, int base_qp, double target_bits, int verbose) {
    const int npix = w * h;
    /* bitstream worst-case is above 8bpp for tiny QP; allocate 4x raw */
    size_t cap = (size_t)n * (size_t)npix * 4u;
    uint8_t *stream = malloc(cap);
    uint8_t *drec = malloc((size_t)n * (size_t)npix);
    if (!stream || !drec) { free(stream); free(drec); return -1; }

    bitstream enc;
    bs_init(&enc, stream, (int)cap);

    bs_put(&enc, (unsigned)w, 16);
    bs_put(&enc, (unsigned)h, 16);
    bs_put(&enc, (unsigned)n, 16);
    bs_put(&enc, (unsigned)gop, 8);
    bs_put(&enc, (unsigned)base_qp, 8);

    int qp = base_qp;
    int qp_min = qp, qp_max = qp;
    int total_bits = 0;
    int verbose_every = (n > 16 && verbose) ? (n / 8) : 1;

    for (int f = 0; f < n; f++) {
        int is_i = (f % gop) == 0;
        bs_put(&enc, (unsigned)qp, 6);
        const uint8_t *src = frames + (size_t)f * npix;
        uint8_t *dst = rec + (size_t)f * npix;
        int bits;
        if (is_i)
            bits = frame_i_encode(src, dst, w, h, qp, &enc);
        else
            bits = frame_p_encode(rec + (size_t)(f - 1) * npix, src, dst,
                                  w, h, qp, &enc);
        if (bits < 0) { free(stream); free(drec); return -1; }
        total_bits = enc.bitpos;
        if (verbose && (f % verbose_every == 0 || f == n - 1)) {
            double psnr = frame_psnr(src, dst, npix);
            printf("    frame %3d %s qp=%2d frame_bits=%6d PSNR=%5.2f dB\n",
                   f, is_i ? "I" : "P", qp, bits, psnr);
        }
        if (target_bits > 0) {
            if (bits > target_bits * 1.15) {
                if (qp < 40) qp++;
            } else if (bits < target_bits * 0.85) {
                if (qp > 1) qp--;
            }
        }
        if (qp < qp_min) qp_min = qp;
        if (qp > qp_max) qp_max = qp;
    }
    int stream_bytes = bs_bytes(&enc);

    /* Closed-loop decode from bitstream */
    bitstream dec;
    bs_open(&dec, stream, stream_bytes);
    int dw = bs_get(&dec, 16);
    int dh = bs_get(&dec, 16);
    int dn = bs_get(&dec, 16);
    int dgop = bs_get(&dec, 8);
    int dqp0 = bs_get(&dec, 8);
    (void)dqp0;
    if (dw != w || dh != h || dn != n || dgop != gop) {
        free(stream); free(drec); return -1;
    }

    double sum_dec = 0, min_dec = 1e9;
    for (int f = 0; f < n; f++) {
        int fqp = bs_get(&dec, 6);
        if (fqp < 0) { free(stream); free(drec); return -1; }
        int is_i = (f % gop) == 0;
        uint8_t *dst = drec + (size_t)f * npix;
        const uint8_t *src = frames + (size_t)f * npix;
        int rc;
        if (is_i)
            rc = frame_i_decode(dst, w, h, fqp, &dec);
        else
            rc = frame_p_decode(drec + (size_t)(f - 1) * npix, dst, w, h, fqp, &dec);
        if (rc != 0) { free(stream); free(drec); return -1; }
        double p = frame_psnr(src, dst, npix);
        sum_dec += p;
        if (p < min_dec) min_dec = p;
    }

    double avg = sum_dec / n;
    double bpp = (double)total_bits / ((double)npix * n);
    if (verbose) {
        printf("  info: stream=%d bytes  total_bits=%d  bpp=%.3f  raw_bpp=8.0  compression=%.2fx\n",
               stream_bytes, total_bits, bpp, 8.0 / (bpp + 1e-12));
        printf("  info: closed-loop decode avg_PSNR=%.2f dB  min_PSNR=%.2f dB\n",
               avg, min_dec);
        if (target_bits > 0)
            printf("  info: rate-control qp walked [%d..%d]  target_bits/frame=%.0f\n",
                   qp_min, qp_max, target_bits);
    }

    memcpy(rec, drec, (size_t)n * (size_t)npix);
    free(stream);
    free(drec);
    return avg;
}
