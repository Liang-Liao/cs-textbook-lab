/* copied from lab11-color-yuv */
#ifndef LAB12_PPM_IO_H
#define LAB12_PPM_IO_H

#include <stdint.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h);
int pgm_write(const char *path, const uint8_t *gray, int w, int h);

/* Readers with header validation (magic P5/P6, whitespace/comment handling,
 * dimension + maxval parse). Pass rgb/gray = NULL to probe dimensions only.
 * cap_bytes guards the caller's buffer. Returns 0 on success. */
int ppm_read(const char *path, uint8_t *rgb, int cap_bytes, int *w, int *h);
int pgm_read(const char *path, uint8_t *gray, int cap_bytes, int *w, int *h);

#endif
