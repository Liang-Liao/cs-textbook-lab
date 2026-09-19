#include "edge.h"

#include <math.h>
#include <stdlib.h>

static double sample_rep(const uint8_t *src, int w, int h, int x, int y) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= w) x = w - 1;
    if (y >= h) y = h - 1;
    return (double)src[y * w + x];
}

int sobel_magnitude(const uint8_t *src, int w, int h, uint8_t *mag) {
    if (!src || !mag || w <= 0 || h <= 0) return -1;
    size_t n = (size_t)w * (size_t)h;
    double *tmp = (double *)malloc(n * sizeof(double));
    if (!tmp) return -2;
    double peak = 1e-12;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double gx = -sample_rep(src, w, h, x - 1, y - 1) + sample_rep(src, w, h, x + 1, y - 1) -
                        2.0 * sample_rep(src, w, h, x - 1, y) + 2.0 * sample_rep(src, w, h, x + 1, y) -
                        sample_rep(src, w, h, x - 1, y + 1) + sample_rep(src, w, h, x + 1, y + 1);
            double gy = -sample_rep(src, w, h, x - 1, y - 1) - 2.0 * sample_rep(src, w, h, x, y - 1) -
                        sample_rep(src, w, h, x + 1, y - 1) + sample_rep(src, w, h, x - 1, y + 1) +
                        2.0 * sample_rep(src, w, h, x, y + 1) + sample_rep(src, w, h, x + 1, y + 1);
            double m = sqrt(gx * gx + gy * gy);
            tmp[y * w + x] = m;
            if (m > peak) peak = m;
        }
    }
    for (size_t i = 0; i < n; i++) {
        double v = tmp[i] / peak * 255.0;
        if (v > 255.0) v = 255.0;
        mag[i] = (uint8_t)(v + 0.5);
    }
    free(tmp);
    return 0;
}
