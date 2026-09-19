#ifndef LAB12_METRICS_H
#define LAB12_METRICS_H

#include <stdint.h>

double psnr_u8(const uint8_t *a, const uint8_t *b, int n);
double ssim_u8(const uint8_t *a, const uint8_t *b, int w, int h, int win);

#endif
