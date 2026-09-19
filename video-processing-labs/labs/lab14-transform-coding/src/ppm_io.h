/* copied from lab11-color-yuv */
#ifndef LAB14_PPM_H
#define LAB14_PPM_H
#include <stdint.h>
int ppm_write_gray(const char *path, const uint8_t *p, int w, int h);
#endif
