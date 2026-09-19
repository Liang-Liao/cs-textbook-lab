#ifndef LAB12_RESIZE_H
#define LAB12_RESIZE_H

#include <stdint.h>

double bilinear_sample(const double *src, int w, int h, double x, double y);
int bilinear_resize2x(const uint8_t *src, int w, int h, uint8_t *dst);
int nearest_resize2x(const uint8_t *src, int w, int h, uint8_t *dst);

#endif
