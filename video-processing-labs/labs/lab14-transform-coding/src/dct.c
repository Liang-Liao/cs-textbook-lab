#include "dct.h"
#include <math.h>

void dct8(const double in[64], double out[64]) {
    for (int u = 0; u < 8; u++) {
        for (int v = 0; v < 8; v++) {
            double s = 0.0;
            for (int x = 0; x < 8; x++) {
                for (int y = 0; y < 8; y++) {
                    s += in[y * 8 + x] *
                         cos((2 * x + 1) * u * M_PI / 16.0) *
                         cos((2 * y + 1) * v * M_PI / 16.0);
                }
            }
            double cu = (u == 0) ? sqrt(0.5) : 1.0;
            double cv = (v == 0) ? sqrt(0.5) : 1.0;
            out[v * 8 + u] = 0.25 * cu * cv * s;
        }
    }
}

void idct8(const double in[64], double out[64]) {
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            double s = 0.0;
            for (int u = 0; u < 8; u++) {
                for (int v = 0; v < 8; v++) {
                    double cu = (u == 0) ? sqrt(0.5) : 1.0;
                    double cv = (v == 0) ? sqrt(0.5) : 1.0;
                    s += cu * cv * in[v * 8 + u] *
                         cos((2 * x + 1) * u * M_PI / 16.0) *
                         cos((2 * y + 1) * v * M_PI / 16.0);
                }
            }
            out[y * 8 + x] = 0.25 * s;
        }
    }
}

void quant8(const double coef[64], int qp, int q[64]) {
    if (qp < 1) qp = 1;
    if (qp > 51) qp = 51;
    double step = (double)qp;
    for (int i = 0; i < 64; i++) q[i] = (int)lround(coef[i] / step);
}

void dequant8(const int q[64], int qp, double coef[64]) {
    if (qp < 1) qp = 1;
    if (qp > 51) qp = 51;
    double step = (double)qp;
    for (int i = 0; i < 64; i++) coef[i] = q[i] * step;
}

/* JPEG-style luminance perceptual weighting: coarse steps at high spatial
 * frequencies where the eye is least sensitive. quality 1..100 (50 = table
 * as published). */
static const int QLUM[64] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99,
};

static int qstep_matrix(int k, int quality) {
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    int scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);
    int s = (QLUM[k] * scale + 50) / 100;
    if (s < 1) s = 1;
    return s;
}

void quant8_matrix(const double coef[64], int quality, int q[64]) {
    for (int i = 0; i < 64; i++)
        q[i] = (int)lround(coef[i] / (double)qstep_matrix(i, quality));
}

void dequant8_matrix(const int q[64], int quality, double coef[64]) {
    for (int i = 0; i < 64; i++)
        coef[i] = q[i] * (double)qstep_matrix(i, quality);
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

void zigzag(const int q[64], int zz[64]) {
    for (int i = 0; i < 64; i++) zz[i] = q[ZZ[i]];
}

void unzigzag(const int zz[64], int q[64]) {
    for (int i = 0; i < 64; i++) q[ZZ[i]] = zz[i];
}
