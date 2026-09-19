/* copied from lab01-sampling-quantization */
#ifndef LAB04_PGM_IO_H
#define LAB04_PGM_IO_H
#include <stdint.h>
int pgm_write(const char *path, const uint8_t *pixels, int w, int h);
#endif
