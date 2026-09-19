#include "hist.h"

void hist_of_u8(const uint8_t *img, int n, int hist[256]) {
    for (int i = 0; i < 256; i++) hist[i] = 0;
    for (int i = 0; i < n; i++) hist[img[i]]++;
}

void hist_equalize_u8(uint8_t *img, int n) {
    int hist[256];
    uint8_t map[256];
    hist_of_u8(img, n, hist);
    int first = 0;
    while (first < 256 && hist[first] == 0) first++;
    int c0 = hist[first];
    int denom = n - c0;
    if (denom <= 0) return;
    int run = 0;
    for (int i = 0; i < 256; i++) {
        run += hist[i];
        double t = (double)(run - c0) / (double)denom;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        map[i] = (uint8_t)(t * 255.0 + 0.5);
    }
    for (int i = 0; i < n; i++) img[i] = map[img[i]];
}

void hist_render_pgm(const int hist[256], uint8_t *img, int w, int h) {
    int hmax = 1;
    for (int i = 0; i < 256; i++) {
        if (hist[i] > hmax) hmax = hist[i];
    }
    for (int col = 0; col < w; col++) {
        int bin = col * 256 / w;
        if (bin > 255) bin = 255;
        int colh = (int)((long long)hist[bin] * (h - 1) / hmax);
        for (int row = 0; row < h; row++) {
            int from_bottom = h - 1 - row;
            img[row * w + col] = (from_bottom < colh) ? (uint8_t)220 : (uint8_t)20;
        }
    }
}
