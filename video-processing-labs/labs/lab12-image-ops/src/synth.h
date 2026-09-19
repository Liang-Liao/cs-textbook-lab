#ifndef LAB12_SYNTH_H
#define LAB12_SYNTH_H

#include <stdint.h>

void synth_checker(uint8_t *img, int w, int h, int cell, uint8_t a, uint8_t b);
void synth_circle(uint8_t *img, int w, int h, int cx, int cy, int radius, uint8_t fg,
                  uint8_t bg);
void synth_gradient(uint8_t *img, int w, int h, uint8_t lo, uint8_t hi);
void synth_ssim_ref(uint8_t *img, int w, int h);
void brightness_shift(const uint8_t *src, uint8_t *dst, int n, int delta);
void pure_gain(const uint8_t *src, uint8_t *dst, int n, double g);
void comb_warp_h(const uint8_t *src, uint8_t *dst, int w, int h, int amp);

#endif
