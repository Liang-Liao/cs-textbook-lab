/* copied from lab11-color-yuv */
#ifndef LAB13_PPM_H
#define LAB13_PPM_H
#include "me.h"
int ppm_write_gray(const char *path, const img *im);
int ppm_write_rgb(const char *path, const unsigned char *rgb, int w, int h);
#endif
