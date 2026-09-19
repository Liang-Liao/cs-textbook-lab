#ifndef LAB11_HIST_H
#define LAB11_HIST_H

#include <stdint.h>

void hist_equalize_u8(uint8_t *img, int n);
void hist_of_u8(const uint8_t *img, int n, int hist[256]);
void hist_render_pgm(const int hist[256], uint8_t *img, int w, int h);

#endif
