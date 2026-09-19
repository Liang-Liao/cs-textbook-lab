/* copied from lab12, trimmed */
#include "ppm_io.h"
#include <stdio.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(rgb, 1, (size_t)w * h * 3, f);
    int werr = ferror(f);
    fclose(f);
    return werr ? -6 : 0;
}
