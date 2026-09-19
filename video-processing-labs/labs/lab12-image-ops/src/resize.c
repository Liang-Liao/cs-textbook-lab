#include "resize.h"

#include <stdlib.h>

double bilinear_sample(const double *src, int w, int h, double x, double y) {
    if (x < 0.0) x = 0.0;
    if (y < 0.0) y = 0.0;
    if (x > (double)(w - 1)) x = (double)(w - 1);
    if (y > (double)(h - 1)) y = (double)(h - 1);
    int x0 = (int)x;
    int y0 = (int)y;
    int x1 = (x0 + 1 < w) ? x0 + 1 : w - 1;
    int y1 = (y0 + 1 < h) ? y0 + 1 : h - 1;
    double fx = x - (double)x0;
    double fy = y - (double)y0;
    double a = src[y0 * w + x0] * (1.0 - fx) + src[y0 * w + x1] * fx;
    double b = src[y1 * w + x0] * (1.0 - fx) + src[y1 * w + x1] * fx;
    return a * (1.0 - fy) + b * fy;
}

int bilinear_resize2x(const uint8_t *src, int w, int h, uint8_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return -1;
    size_t n = (size_t)w * (size_t)h;
    double *tmp = (double *)malloc(n * sizeof(double));
    if (!tmp) return -2;
    for (size_t i = 0; i < n; i++) tmp[i] = (double)src[i];
    int dw = w * 2;
    int dh = h * 2;
    for (int dy = 0; dy < dh; dy++) {
        double sy = (double)dy * 0.5;
        for (int dx = 0; dx < dw; dx++) {
            double sx = (double)dx * 0.5;
            double v = bilinear_sample(tmp, w, h, sx, sy);
            if (v < 0.0) v = 0.0;
            if (v > 255.0) v = 255.0;
            dst[dy * dw + dx] = (uint8_t)(v + 0.5);
        }
    }
    free(tmp);
    return 0;
}

int nearest_resize2x(const uint8_t *src, int w, int h, uint8_t *dst) {
    if (!src || !dst || w <= 0 || h <= 0) return -1;
    int dw = w * 2;
    int dh = h * 2;
    for (int dy = 0; dy < dh; dy++) {
        int sy = dy / 2;
        for (int dx = 0; dx < dw; dx++) {
            int sx = dx / 2;
            dst[dy * dw + dx] = src[sy * w + sx];
        }
    }
    return 0;
}
