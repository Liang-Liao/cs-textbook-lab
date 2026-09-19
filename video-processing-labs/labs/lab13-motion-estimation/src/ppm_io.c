/* copied from lab11-color-yuv (adapted for lab13-motion-estimation) */
#include "ppm_io.h"
#include <stdio.h>
int ppm_write_gray(const char *path, const img *im) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P5\n%d %d\n255\n", im->w, im->h);
    fwrite(im->p, 1, (size_t)im->w * im->h, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
int ppm_write_rgb(const char *path, const unsigned char *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
