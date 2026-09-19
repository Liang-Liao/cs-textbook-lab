/* copied from lab11-color-yuv (adapted for lab14-transform-coding) */
#include "ppm_io.h"
#include <stdio.h>
int ppm_write_gray(const char *path, const uint8_t *p, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(p, 1, (size_t)w * h, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
