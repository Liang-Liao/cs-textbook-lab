#include "synth.h"

static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void synth_checker(uint8_t *img, int w, int h, int cell, uint8_t a, uint8_t b) {
    if (cell < 1) cell = 1;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int cx = col / cell;
            int cy = row / cell;
            img[row * w + col] = ((cx + cy) & 1) ? b : a;
        }
    }
}

void synth_circle(uint8_t *img, int w, int h, int cx, int cy, int radius, uint8_t fg,
                  uint8_t bg) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            double dx = (double)(col - cx);
            double dy = (double)(row - cy);
            img[row * w + col] = (dx * dx + dy * dy <= (double)radius * radius) ? fg : bg;
        }
    }
}

void synth_gradient(uint8_t *img, int w, int h, uint8_t lo, uint8_t hi) {
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            double t = (w > 1) ? (double)col / (double)(w - 1) : 0.0;
            img[row * w + col] = (uint8_t)((double)lo + ((double)hi - (double)lo) * t + 0.5);
        }
    }
}

void synth_ssim_ref(uint8_t *img, int w, int h) {
    /* left: smooth gradient; right: mild checker with cell=2 (1px warp kills structure) */
    int mid = w / 2;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            if (col < mid) {
                double t = (mid > 1) ? (double)col / (double)(mid - 1) : 0.0;
                img[row * w + col] = (uint8_t)(40.0 + 160.0 * t + 0.5);
            } else {
                int cx = col / 2;
                int cy = row / 2;
                img[row * w + col] = ((cx + cy) & 1) ? (uint8_t)140 : (uint8_t)100;
            }
        }
    }
}

void brightness_shift(const uint8_t *src, uint8_t *dst, int n, int delta) {
    for (int i = 0; i < n; i++) dst[i] = clamp_u8((int)src[i] + delta);
}

void pure_gain(const uint8_t *src, uint8_t *dst, int n, double g) {
    for (int i = 0; i < n; i++) {
        double v = (double)src[i] * g;
        dst[i] = clamp_u8((int)(v + 0.5));
    }
}

void comb_warp_h(const uint8_t *src, uint8_t *dst, int w, int h, int amp) {
    /* odd/even rows shift opposite directions: geometric structure damage */
    for (int row = 0; row < h; row++) {
        int shift = ((row & 1) ? -amp : amp);
        for (int col = 0; col < w; col++) {
            int sx = col + shift;
            if (sx < 0) sx = 0;
            if (sx >= w) sx = w - 1;
            dst[row * w + col] = src[row * w + sx];
        }
    }
}
