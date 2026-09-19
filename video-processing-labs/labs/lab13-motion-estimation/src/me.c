#include "me.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

double wall_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return 1000.0 * c.QuadPart / f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000.0 * ts.tv_sec + 1e-6 * ts.tv_nsec;
#endif
}

img img_new(int w, int h) {
    img im;
    im.w = w; im.h = h;
    im.p = calloc((size_t)w * h, 1);
    return im;
}
void img_free(img *im) { free(im->p); im->p = NULL; }
img img_clone(img src) {
    img d = img_new(src.w, src.h);
    if (d.p) memcpy(d.p, src.p, (size_t)src.w * src.h);
    return d;
}
void img_shift(img src, img *dst, int dx, int dy) {
    memset(dst->p, 0, (size_t)src.w * src.h);
    for (int y = 0; y < src.h; y++) {
        int sy = y - dy;
        if (sy < 0 || sy >= src.h) continue;
        for (int x = 0; x < src.w; x++) {
            int sx = x - dx;
            if (sx < 0 || sx >= src.w) continue;
            dst->p[y * src.w + x] = src.p[sy * src.w + sx];
        }
    }
}

double sad_block(const img *a, const img *b, int bx, int by, int bs) {
    double s = 0;
    for (int y = 0; y < bs; y++) {
        int yy = by + y;
        if (yy < 0 || yy >= a->h) continue;
        for (int x = 0; x < bs; x++) {
            int xx = bx + x;
            if (xx < 0 || xx >= a->w) continue;
            s += fabs((double)a->p[yy * a->w + xx] - (double)b->p[yy * b->w + xx]);
        }
    }
    return s;
}

static double sad_at(const img *ref, const img *cur, int bx, int by, int bs,
                     int mvx, int mvy) {
    double s = 0;
    int cnt = 0;
    for (int y = 0; y < bs; y++) {
        int cy = by + y, ry = by + y + mvy;
        if (cy < 0 || cy >= cur->h || ry < 0 || ry >= ref->h) continue;
        for (int x = 0; x < bs; x++) {
            int cx = bx + x, rx = bx + x + mvx;
            if (cx < 0 || cx >= cur->w || rx < 0 || rx >= ref->w) continue;
            s += fabs((double)ref->p[ry * ref->w + rx] - (double)cur->p[cy * cur->w + cx]);
            cnt++;
        }
    }
    if (cnt < (bs * bs) / 2) return 1e300;
    return s;
}

mv me_full(const img *ref, const img *cur, int bx, int by, int bs, int range) {
    mv best = {0, 0};
    double bv = 1e300;
    for (int my = -range; my <= range; my++) {
        for (int mx = -range; mx <= range; mx++) {
            double s = sad_at(ref, cur, bx, by, bs, mx, my);
            if (s < bv) { bv = s; best.x = mx; best.y = my; }
        }
    }
    return best;
}

mv me_diamond(const img *ref, const img *cur, int bx, int by, int bs, int range) {
    mv best = {0, 0};
    double bv = sad_at(ref, cur, bx, by, bs, 0, 0);
    int steps[3] = {4, 2, 1};
    for (int si = 0; si < 3; si++) {
        int st = steps[si];
        if (st > range) st = range;
        if (st < 1) st = 1;
        int improved = 1;
        while (improved) {
            improved = 0;
            mv center = best;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int mx = center.x + dx * st;
                    int my = center.y + dy * st;
                    if (mx < -range || mx > range || my < -range || my > range) continue;
                    double s = sad_at(ref, cur, bx, by, bs, mx, my);
                    if (s < bv) { bv = s; best.x = mx; best.y = my; improved = 1; }
                }
            }
        }
    }
    for (int my = best.y - 2; my <= best.y + 2; my++) {
        for (int mx = best.x - 2; mx <= best.x + 2; mx++) {
            if (mx < -range || mx > range || my < -range || my > range) continue;
            double s = sad_at(ref, cur, bx, by, bs, mx, my);
            if (s < bv) { bv = s; best.x = mx; best.y = my; }
        }
    }
    return best;
}

/* Classic Three-Step Search (Koga et al. 1981).
 * Start at (0,0); step sizes are the largest power of two ≤ range, then
 * halved each stage until 1. Each stage tests the 8 neighbours at ±step
 * around the current best. Complexity ≈ 8·log2(range)+1 SADs vs (2R+1)². */
mv me_tss(const img *ref, const img *cur, int bx, int by, int bs, int range) {
    mv best = {0, 0};
    if (range < 1) return best;
    double bv = sad_at(ref, cur, bx, by, bs, 0, 0);
    int step = 1;
    while ((step << 1) <= range) step <<= 1;
    while (step >= 1) {
        mv center = best;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int mx = center.x + dx * step;
                int my = center.y + dy * step;
                if (mx < -range || mx > range || my < -range || my > range) continue;
                double s = sad_at(ref, cur, bx, by, bs, mx, my);
                if (s < bv) { bv = s; best.x = mx; best.y = my; }
            }
        }
        step >>= 1;
    }
    return best;
}

double img_sample(const img *im, double x, double y) {
    int x0 = (int)floor(x), y0 = (int)floor(y);
    double fx = x - x0, fy = y - y0;
    double a = 0, b = 0, c = 0, d = 0;
    if (x0 >= 0 && y0 >= 0 && x0 < im->w && y0 < im->h) a = im->p[y0 * im->w + x0];
    if (x0 + 1 < im->w && y0 >= 0 && y0 < im->h) b = im->p[y0 * im->w + x0 + 1];
    if (x0 >= 0 && y0 + 1 < im->h && x0 < im->w) c = im->p[(y0 + 1) * im->w + x0];
    if (x0 + 1 < im->w && y0 + 1 < im->h) d = im->p[(y0 + 1) * im->w + x0 + 1];
    return (1 - fy) * ((1 - fx) * a + fx * b) + fy * ((1 - fx) * c + fx * d);
}

void mc_block(const img *ref, double mvx, double mvy, int bx, int by, int bs,
              unsigned char *out) {
    for (int y = 0; y < bs; y++) {
        for (int x = 0; x < bs; x++) {
            double v = img_sample(ref, bx + x + mvx, by + y + mvy);
            if (v < 0) v = 0;
            if (v > 255) v = 255;
            out[y * bs + x] = (unsigned char)lrint(v);
        }
    }
}
