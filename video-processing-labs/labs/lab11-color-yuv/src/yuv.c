#include "yuv.h"

#include <stdlib.h>

static double clip_u8(double v) {
    if (v < 0.0) return 0.0;
    if (v > 255.0) return 255.0;
    return v;
}

static void mat3_invert(const double src[3][3], double dst[3][3]) {
    double det = src[0][0] * (src[1][1] * src[2][2] - src[1][2] * src[2][1]) -
                 src[0][1] * (src[1][0] * src[2][2] - src[1][2] * src[2][0]) +
                 src[0][2] * (src[1][0] * src[2][1] - src[1][1] * src[2][0]);
    double id = 1.0 / det;
    dst[0][0] = (src[1][1] * src[2][2] - src[1][2] * src[2][1]) * id;
    dst[0][1] = -(src[0][1] * src[2][2] - src[0][2] * src[2][1]) * id;
    dst[0][2] = (src[0][1] * src[1][2] - src[0][2] * src[1][1]) * id;
    dst[1][0] = -(src[1][0] * src[2][2] - src[1][2] * src[2][0]) * id;
    dst[1][1] = (src[0][0] * src[2][2] - src[0][2] * src[2][0]) * id;
    dst[1][2] = -(src[0][0] * src[1][2] - src[0][2] * src[1][0]) * id;
    dst[2][0] = (src[1][0] * src[2][1] - src[1][1] * src[2][0]) * id;
    dst[2][1] = -(src[0][0] * src[2][1] - src[0][1] * src[2][0]) * id;
    dst[2][2] = (src[0][0] * src[1][1] - src[0][1] * src[1][0]) * id;
}

void yuv_matrix_init(yuv_matrix *m, yuv_std_t std) {
    double c[3][3];
    if (std == YUV_STD_BT709) {
        /* ITU-R BT.709 limited range, RGB 0..255 */
        c[0][0] = 46.5590 / 255.0;
        c[0][1] = 157.5400 / 255.0;
        c[0][2] = 15.9010 / 255.0;
        c[1][0] = -25.5560 / 255.0;
        c[1][1] = -86.9460 / 255.0;
        c[1][2] = 112.5020 / 255.0;
        c[2][0] = 112.5020 / 255.0;
        c[2][1] = -102.1590 / 255.0;
        c[2][2] = -10.3430 / 255.0;
    } else {
        /* ITU-R BT.601 limited range, RGB 0..255 */
        c[0][0] = 65.4810 / 255.0;
        c[0][1] = 128.5530 / 255.0;
        c[0][2] = 24.9660 / 255.0;
        c[1][0] = -37.7970 / 255.0;
        c[1][1] = -74.2030 / 255.0;
        c[1][2] = 112.0000 / 255.0;
        c[2][0] = 112.0000 / 255.0;
        c[2][1] = -93.7860 / 255.0;
        c[2][2] = -18.2140 / 255.0;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) m->fwd[i][j] = c[i][j];
    }
    m->off_fwd[0] = 16.0;
    m->off_fwd[1] = 128.0;
    m->off_fwd[2] = 128.0;
    mat3_invert(m->fwd, m->inv);
    for (int i = 0; i < 3; i++) {
        double s = 0.0;
        for (int j = 0; j < 3; j++) s += m->inv[i][j] * m->off_fwd[j];
        m->off_inv[i] = -s;
    }
}

void rgb_to_yuv(const yuv_matrix *m, double r, double g, double b,
                double *y, double *u, double *v) {
    double rgb[3] = {r, g, b};
    double out[3];
    for (int i = 0; i < 3; i++) {
        out[i] = m->off_fwd[i] + m->fwd[i][0] * rgb[0] + m->fwd[i][1] * rgb[1] +
                 m->fwd[i][2] * rgb[2];
    }
    *y = out[0];
    *u = out[1];
    *v = out[2];
}

void yuv_to_rgb(const yuv_matrix *m, double y, double u, double v,
                double *r, double *g, double *b) {
    double yuv[3] = {y, u, v};
    double out[3];
    for (int i = 0; i < 3; i++) {
        out[i] = m->off_inv[i] + m->inv[i][0] * yuv[0] + m->inv[i][1] * yuv[1] +
                 m->inv[i][2] * yuv[2];
    }
    *r = clip_u8(out[0]);
    *g = clip_u8(out[1]);
    *b = clip_u8(out[2]);
}

int rgb8_to_yuv444(const yuv_matrix *m, const uint8_t *rgb, int w, int h,
                   double *y, double *u, double *v) {
    if (!m || !rgb || !y || !u || !v || w <= 0 || h <= 0) return -1;
    for (int i = 0; i < w * h; i++) {
        rgb_to_yuv(m, (double)rgb[3 * i], (double)rgb[3 * i + 1], (double)rgb[3 * i + 2],
                   &y[i], &u[i], &v[i]);
    }
    return 0;
}

int yuv444_to_rgb8(const yuv_matrix *m, const double *y, const double *u,
                   const double *v, int w, int h, uint8_t *rgb) {
    if (!m || !y || !u || !v || !rgb || w <= 0 || h <= 0) return -1;
    for (int i = 0; i < w * h; i++) {
        double r, g, b;
        yuv_to_rgb(m, y[i], u[i], v[i], &r, &g, &b);
        rgb[3 * i] = (uint8_t)(r + 0.5);
        rgb[3 * i + 1] = (uint8_t)(g + 0.5);
        rgb[3 * i + 2] = (uint8_t)(b + 0.5);
    }
    return 0;
}

int yuv420_alloc(yuv420 *p, int w, int h) {
    if (!p || w <= 0 || h <= 0 || (w % 2) || (h % 2)) return -1;
    p->w = w;
    p->h = h;
    p->y = (double *)malloc((size_t)w * (size_t)h * sizeof(double));
    p->u = (double *)malloc((size_t)(w / 2) * (size_t)(h / 2) * sizeof(double));
    p->v = (double *)malloc((size_t)(w / 2) * (size_t)(h / 2) * sizeof(double));
    if (!p->y || !p->u || !p->v) {
        yuv420_free(p);
        return -2;
    }
    return 0;
}

void yuv420_free(yuv420 *p) {
    if (!p) return;
    free(p->y);
    free(p->u);
    free(p->v);
    p->y = NULL;
    p->u = NULL;
    p->v = NULL;
}

int rgb8_to_yuv420(const yuv_matrix *m, const uint8_t *rgb, int w, int h, yuv420 *p) {
    if (!m || !rgb || !p || w <= 0 || h <= 0 || (w % 2) || (h % 2)) return -1;
    if (p->w != w || p->h != h || !p->y) {
        yuv420_free(p);
        if (yuv420_alloc(p, w, h) != 0) return -2;
    }
    int cw = w / 2;
    int ch = h / 2;
    /* Y full-res; chroma 2x2 average into half-res planes */
    for (int cr = 0; cr < ch; cr++) {
        for (int cc = 0; cc < cw; cc++) {
            double su = 0.0, sv = 0.0;
            for (int dy = 0; dy < 2; dy++) {
                for (int dx = 0; dx < 2; dx++) {
                    int row = 2 * cr + dy;
                    int col = 2 * cc + dx;
                    int i = row * w + col;
                    double yy, uu, vv;
                    rgb_to_yuv(m, (double)rgb[3 * i], (double)rgb[3 * i + 1],
                               (double)rgb[3 * i + 2], &yy, &uu, &vv);
                    p->y[i] = yy;
                    su += uu;
                    sv += vv;
                }
            }
            p->u[cr * cw + cc] = su * 0.25;
            p->v[cr * cw + cc] = sv * 0.25;
        }
    }
    return 0;
}

int yuv420_to_rgb8(const yuv_matrix *m, const yuv420 *p, uint8_t *rgb) {
    if (!m || !p || !p->y || !p->u || !p->v || !rgb) return -1;
    int w = p->w, h = p->h;
    int cw = w / 2;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int i = row * w + col;
            int ci = (row / 2) * cw + (col / 2);
            double r, g, b;
            yuv_to_rgb(m, p->y[i], p->u[ci], p->v[ci], &r, &g, &b);
            rgb[3 * i] = (uint8_t)(r + 0.5);
            rgb[3 * i + 1] = (uint8_t)(g + 0.5);
            rgb[3 * i + 2] = (uint8_t)(b + 0.5);
        }
    }
    return 0;
}

void chroma_upsample_nearest(const double *ch, int cw, int chh, double *out, int w, int h) {
    for (int row = 0; row < h; row++) {
        int cr = row / 2;
        if (cr >= chh) cr = chh - 1;
        for (int col = 0; col < w; col++) {
            int cc = col / 2;
            if (cc >= cw) cc = cw - 1;
            out[row * w + col] = ch[cr * cw + cc];
        }
    }
}
