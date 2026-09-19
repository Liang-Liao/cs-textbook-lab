#ifndef LAB12_EDGE_H
#define LAB12_EDGE_H

#include <stdint.h>

int sobel_magnitude(const uint8_t *src, int w, int h, uint8_t *mag);

#endif
