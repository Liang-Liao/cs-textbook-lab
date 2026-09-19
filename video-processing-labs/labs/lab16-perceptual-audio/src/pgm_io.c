#include "pgm_io.h"
#include <stdio.h>

int pgm_write(const char *path, const uint8_t *pixels, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(pixels, 1, (size_t)w * (size_t)h, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
