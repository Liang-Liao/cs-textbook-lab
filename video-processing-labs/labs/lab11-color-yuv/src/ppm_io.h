#ifndef LAB11_PPM_IO_H
#define LAB11_PPM_IO_H

#include <stdint.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h);
int pgm_write(const char *path, const uint8_t *gray, int w, int h);

#endif
