/* Minimal I/P video codec for lab19 E2E (adapted from lab15). */
#include "vid_codec.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void dct8(const double in[64], double out[64]) {
    for (int u = 0; u < 8; u++) for (int v = 0; v < 8; v++) {
        double s = 0;
        for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++)
            s += in[y * 8 + x] * cos((2 * x + 1) * u * M_PI / 16) *
                 cos((2 * y + 1) * v * M_PI / 16);
        double cu = u ? 1 : sqrt(0.5), cv = v ? 1 : sqrt(0.5);
        out[v * 8 + u] = 0.25 * cu * cv * s;
    }
}

static void idct8(const double in[64], double out[64]) {
    for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++) {
        double s = 0;
        for (int u = 0; u < 8; u++) for (int v = 0; v < 8; v++) {
            double cu = u ? 1 : sqrt(0.5), cv = v ? 1 : sqrt(0.5);
            s += cu * cv * in[v * 8 + u] * cos((2 * x + 1) * u * M_PI / 16) *
                 cos((2 * y + 1) * v * M_PI / 16);
        }
        out[y * 8 + x] = 0.25 * s;
    }
}

static const int ZZ[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

void vid_seq_make(uint8_t *img, int w, int h, int t) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double v = 100.0 + 30.0 * sin(0.05 * x + 0.03 * t) * cos(0.04 * y) +
                       15.0 * cos(0.02 * (x + y) + 0.05 * t);
            /* slowly translating box */
            int sx = (8 + t) % (w > 24 ? w - 24 : 1);
            int sy = (6 + (t / 2)) % (h > 20 ? h - 20 : 1);
            if (x >= sx && x < sx + 20 && y >= sy && y < sy + 16) v = 200.0;
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            img[y * w + x] = (uint8_t)(v + 0.5);
        }
    }
}

typedef struct {
    uint8_t *buf;
    int cap;
    int pos;
} wbuf;

static void wb_put(wbuf *w, unsigned v, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        if (w->pos >= w->cap * 8) return;
        int bp = w->pos++;
        if ((v >> i) & 1u) w->buf[bp >> 3] |= (uint8_t)(1u << (7 - (bp & 7)));
    }
}

static int rb_get(const uint8_t *buf, int cap, int *pos, int nbits) {
    unsigned v = 0;
    for (int i = 0; i < nbits; i++) {
        int bp = (*pos)++;
        if ((bp >> 3) >= cap) return -1;
        v = (v << 1) | ((buf[bp >> 3] >> (7 - (bp & 7))) & 1u);
    }
    return (int)v;
}

static void quant_zz(const double coef[64], int qp, int zz[64]) {
    if (qp < 1) qp = 1;
    int tmp[64];
    for (int i = 0; i < 64; i++) tmp[i] = (int)lrint(coef[i] / (double)qp);
    for (int i = 0; i < 64; i++) zz[i] = tmp[ZZ[i]];
}

static void dequant_unzz(const int zz[64], int qp, double dq[64]) {
    if (qp < 1) qp = 1;
    int tmp[64];
    for (int i = 0; i < 64; i++) tmp[ZZ[i]] = zz[i];
    for (int i = 0; i < 64; i++) dq[i] = (double)tmp[i] * (double)qp;
}

static void wb_put_se(wbuf *w, int v) {
    unsigned u = (v <= 0) ? (unsigned)(-2 * v) : (unsigned)(2 * v - 1);
    unsigned x = u + 1u;
    int n = 0;
    unsigned t = x;
    while (t) { n++; t >>= 1; }
    if (n < 1) n = 1;
    wb_put(w, 0, n - 1);
    wb_put(w, x, n);
}

static int rb_get_se(const uint8_t *buf, int cap, int *pos) {
    int z = 0;
    for (;;) {
        int b = rb_get(buf, cap, pos, 1);
        if (b < 0) return 0x7fffffff;
        if (b == 1) break;
        z++;
        if (z > 30) return 0x7fffffff;
    }
    unsigned info = 0;
    if (z > 0) {
        int rest = rb_get(buf, cap, pos, z);
        if (rest < 0) return 0x7fffffff;
        info = (unsigned)rest;
    }
    unsigned x = (1u << z) | info;
    unsigned u = x - 1u;
    if (u & 1u) return (int)((u + 1u) / 2u);
    return -(int)(u / 2u);
}

static void put_rle(wbuf *w, const int zz[64]) {
    wb_put_se(w, zz[0]);
    int i = 1;
    while (i < 64) {
        int run = 0;
        while (i < 64 && zz[i] == 0) { run++; i++; }
        if (i >= 64) break;
        wb_put(w, 0u, 1);
        wb_put(w, (unsigned)run, 6);
        wb_put_se(w, zz[i]);
        i++;
    }
    wb_put(w, 1u, 1);
}

static int get_rle(const uint8_t *buf, int cap, int *pos, int zz[64]) {
    memset(zz, 0, 64 * sizeof(int));
    int dc = rb_get_se(buf, cap, pos);
    if (dc == 0x7fffffff) return -1;
    zz[0] = dc;
    int i = 1;
    for (;;) {
        int eob = rb_get(buf, cap, pos, 1);
        if (eob < 0) return -1;
        if (eob) return 0;
        int run = rb_get(buf, cap, pos, 6);
        if (run < 0) return -1;
        int level = rb_get_se(buf, cap, pos);
        if (level == 0x7fffffff) return -1;
        i += run;
        if (i >= 64) return -1;
        zz[i++] = level;
    }
}

static void recon_block(const double pred[64], const double res[64],
                        uint8_t *dst, int stride) {
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        double v = pred[y * 8 + x] + res[y * 8 + x];
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        dst[y * stride + x] = (uint8_t)lrint(v);
    }
}

static void pred_i(const uint8_t *rec, int stride, int bx, int by, double pred[64]) {
    /* DC from reconstructed left column + above row (matches encoder/decoder). */
    double s = 0;
    int n = 0;
    if (bx > 0) {
        for (int y = 0; y < 8; y++) { s += rec[(by + y) * stride + (bx - 1)]; n++; }
    }
    if (by > 0) {
        for (int x = 0; x < 8; x++) { s += rec[(by - 1) * stride + (bx + x)]; n++; }
    }
    double dc = (n > 0) ? (s / n) : 128.0;
    for (int i = 0; i < 64; i++) pred[i] = dc;
}

static void pred_p(const uint8_t *ref, int stride, int bx, int by, int mvx, int mvy,
                   double pred[64]) {
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int sx = bx + x + mvx, sy = by + y + mvy;
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            if (sx >= stride) sx = stride - 1; /* stride==w in this lab */
            pred[y * 8 + x] = ref[sy * stride + sx];
        }
    }
}

static void me_block(const uint8_t *ref, const uint8_t *src, int w, int h,
                     int bx, int by, int *mvx, int *mvy) {
    int bestx = 0, besty = 0;
    double best = 1e300;
    for (int my = -4; my <= 4; my++) {
        for (int mx = -4; mx <= 4; mx++) {
            int rx = bx + mx, ry = by + my;
            if (rx < 0 || ry < 0 || rx + 8 > w || ry + 8 > h) continue;
            double s = 0;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    double d = (double)src[(by + y) * w + (bx + x)] -
                               (double)ref[(ry + y) * w + (rx + x)];
                    s += d * d;
                }
            if (s < best) { best = s; bestx = mx; besty = my; }
        }
    }
    *mvx = bestx;
    *mvy = besty;
}

static int encode_frame(const uint8_t *src, uint8_t *dst, int w, int h, int qp,
                        int is_i, const uint8_t *ref, wbuf *wbits) {
    if (!is_i && !ref) return -1;
    if (is_i) memset(dst, 128, (size_t)w * (size_t)h);
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            double pred[64], res[64], coef[64], dq[64], rec[64];
            int mvx = 0, mvy = 0;
            if (is_i) {
                pred_i(dst, w, bx, by, pred);
            } else {
                me_block(ref, src, w, h, bx, by, &mvx, &mvy);
                /* signed 5-bit MVs (range ±4 → offset +16) */
                wb_put(wbits, (unsigned)(mvx + 16), 5);
                wb_put(wbits, (unsigned)(mvy + 16), 5);
                pred_p(ref, w, bx, by, mvx, mvy, pred);
            }
            for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++)
                res[y * 8 + x] = (double)src[(by + y) * w + (bx + x)] - pred[y * 8 + x];
            dct8(res, coef);
            int zz[64];
            quant_zz(coef, qp, zz);
            put_rle(wbits, zz);
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return 0;
}

static int decode_frame(uint8_t *dst, int w, int h, int qp, int is_i,
                        const uint8_t *ref, const uint8_t *buf, int cap, int *pos) {
    if (is_i) memset(dst, 128, (size_t)w * (size_t)h);
    for (int by = 0; by < h; by += 8) {
        for (int bx = 0; bx < w; bx += 8) {
            double pred[64], dq[64], rec[64];
            if (is_i) {
                pred_i(dst, w, bx, by, pred);
            } else {
                int mvx = rb_get(buf, cap, pos, 5);
                int mvy = rb_get(buf, cap, pos, 5);
                if (mvx < 0 || mvy < 0) return -1;
                mvx -= 16;
                mvy -= 16;
                pred_p(ref, w, bx, by, mvx, mvy, pred);
            }
            int zz[64];
            if (get_rle(buf, cap, pos, zz) != 0) return -1;
            dequant_unzz(zz, qp, dq);
            idct8(dq, rec);
            recon_block(pred, rec, dst + by * w + bx, w);
        }
    }
    return 0;
}

double vid_psnr(const uint8_t *a, const uint8_t *b, int npix) {
    double se = 0;
    for (int i = 0; i < npix; i++) {
        double d = (double)a[i] - (double)b[i];
        se += d * d;
    }
    if (se <= 0) return 99.0;
    return 10.0 * log10(255.0 * 255.0 * npix / se);
}

int vid_encode_gop(const uint8_t *frames, uint8_t *rec, int w, int h, int n,
                   int gop, int qp, uint8_t *pktbuf, int pktcap, int *pkt_lens,
                   int max_pkts) {
    const int npix = w * h;
    int npkts = 0;
    int off = 0;
    for (int f = 0; f < n; f++) {
        /* Lab19 video line uses all-intra for a compact, robust E2E demo.
         * (Full I/P lives in lab15.) */
        int is_i = 1;
        (void)gop;
        wbuf wb;
        /* reserve 4 bytes for frame header: qp:6 is_i:1 */
        int hdr = 4;
        if (off + hdr >= pktcap) return -1;
        wb.buf = pktbuf + off + hdr;
        wb.cap = pktcap - off - hdr;
        wb.pos = 0;
        memset(wb.buf, 0, (size_t)wb.cap); /* clear only this packet's payload area start */
        const uint8_t *ref = is_i ? NULL : rec + (size_t)(f - 1) * npix;
        if (encode_frame(frames + (size_t)f * npix, rec + (size_t)f * npix, w, h,
                         qp, is_i, ref, &wb) != 0)
            return -1;
        int payload = (wb.pos + 7) / 8;
        int total = hdr + payload;
        if (npkts >= max_pkts) return -1;
        pktbuf[off + 0] = (uint8_t)((total >> 8) & 0xff);
        pktbuf[off + 1] = (uint8_t)(total & 0xff);
        pktbuf[off + 2] = (uint8_t)qp;
        pktbuf[off + 3] = (uint8_t)(is_i ? 1 : 0);
        pkt_lens[npkts] = total;
        npkts++;
        off += total;
    }
    return npkts;
}

int vid_decode_gop(const uint8_t *pktbuf, const int *pkt_lens, int npkts,
                   uint8_t *rec, int w, int h, int n, int gop) {
    const int npix = w * h;
    int off = 0;
    if (npkts < n) return -1;
    for (int f = 0; f < n; f++) {
        int len = pkt_lens[f];
        if (len < 4) return -2;
        const uint8_t *p = pktbuf + off;
        int declared = ((p[0] & 0xff) << 8) | (p[1] & 0xff);
        if (declared != len) return -3;
        int qp = p[2] & 63;
        int is_i = p[3] & 1;
        (void)gop;
        int pos = 0;
        const uint8_t *ref = is_i ? NULL : rec + (size_t)(f - 1) * npix;
        int drc = decode_frame(rec + (size_t)f * npix, w, h, qp, is_i, ref, p + 4,
                               len - 4, &pos);
        if (drc != 0) {
            fprintf(stderr, "decode fail frame=%d is_i=%d qp=%d len=%d pos=%d\n",
                    f, is_i, qp, len, pos);
            return -4 - f;
        }
        off += len;
    }
    return 0;
}
