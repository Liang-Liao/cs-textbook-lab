#include "ppm_io.h"

#include <stdio.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
    if (!path || !rgb || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * (size_t)h * 3u, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}

int pgm_write(const char *path, const uint8_t *gray, int w, int h) {
    if (!path || !gray || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(gray, 1, (size_t)w * (size_t)h, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
