/* copied from lab11-color-yuv (adapted for lab12-image-ops).
 * Ch12 formalizes the PPM/PGM pair: writers AND readers with header
 * validation (magic, dimensions, maxval) used by the selftest roundtrip. */
#include "ppm_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ppm_write(const char *path, const uint8_t *rgb, int w, int h) {
    if (!path || !rgb || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    size_t want = (size_t)w * (size_t)h * 3u;
    int werr = (fwrite(rgb, 1, want, f) != want) || ferror(f);
    fclose(f);
    return werr ? -3 : 0;
}

int pgm_write(const char *path, const uint8_t *gray, int w, int h) {
    if (!path || !gray || w <= 0 || h <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -2;
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    size_t want = (size_t)w * (size_t)h;
    int werr = (fwrite(gray, 1, want, f) != want) || ferror(f);
    fclose(f);
    return werr ? -3 : 0;
}

/* Parse "P6\n<w> <h>\n255\n" style headers: three integers separated by
 * whitespace, '#' comments allowed, maxval must be 255. On success the file
 * position sits on the first raster byte. */
static int read_header(FILE *f, const char *magic, int *w, int *h) {
    char m[3] = {0, 0, 0};
    m[0] = (char)fgetc(f);
    m[1] = (char)fgetc(f);
    if (m[0] != magic[0] || m[1] != magic[1]) return -1;
    long vals[3] = {0, 0, 0};
    int vi = 0, have = 0;
    for (;;) {
        int c = fgetc(f);
        if (c == EOF) return -1;
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n') {
            }
            if (c == EOF) return -1;
            continue;
        }
        if (c >= '0' && c <= '9') {
            if (vi < 3) vals[vi] = vals[vi] * 10 + (c - '0');
            have = 1;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (have) {
                vi++;
                have = 0;
                if (vi == 3) break;
            }
            continue;
        }
        return -1;
    }
    if (vals[0] <= 0 || vals[1] <= 0 || vals[2] != 255) return -1;
    *w = (int)vals[0];
    *h = (int)vals[1];
    return 0;
}

int ppm_read(const char *path, uint8_t *rgb, int cap_bytes, int *w, int *h) {
    if (!path || !w || !h) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -2;
    int iw, ih;
    if (read_header(f, "P6", &iw, &ih) != 0) {
        fclose(f);
        return -3;
    }
    size_t need = (size_t)iw * (size_t)ih * 3u;
    if (iw <= 0 || ih <= 0 || (rgb && need > (size_t)cap_bytes)) {
        fclose(f);
        return -4;
    }
    if (rgb && fread(rgb, 1, need, f) != need) {
        fclose(f);
        return -5;
    }
    fclose(f);
    if (w) *w = iw;
    if (h) *h = ih;
    return 0;
}

int pgm_read(const char *path, uint8_t *gray, int cap_bytes, int *w, int *h) {
    if (!path || !w || !h) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -2;
    int iw, ih;
    if (read_header(f, "P5", &iw, &ih) != 0) {
        fclose(f);
        return -3;
    }
    size_t need = (size_t)iw * (size_t)ih;
    if (iw <= 0 || ih <= 0 || (gray && need > (size_t)cap_bytes)) {
        fclose(f);
        return -4;
    }
    if (gray && fread(gray, 1, need, f) != need) {
        fclose(f);
        return -5;
    }
    fclose(f);
    if (w) *w = iw;
    if (h) *h = ih;
    return 0;
}
