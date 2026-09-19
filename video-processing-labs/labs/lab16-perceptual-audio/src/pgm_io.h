#ifndef LAB16_PGM_IO_H
#define LAB16_PGM_IO_H
#include <stdint.h>
int pgm_write(const char *path, const uint8_t *pixels, int w, int h);
#endif
