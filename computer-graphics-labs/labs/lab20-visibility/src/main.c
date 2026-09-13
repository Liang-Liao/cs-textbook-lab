#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

typedef struct {
  float x, y, z;
  cgl_vec3 col;
} V;

static float edgef(const V *a, const V *b, float x, float y) {
  return (x - a->x) * (b->y - a->y) - (y - a->y) * (b->x - a->x);
}

static int bary(const V *a, const V *b, const V *c, float x, float y, float *w0,
                float *w1, float *w2) {
  float area = edgef(a, b, c->x, c->y);
  if (fabsf(area) < 1e-12f) {
    return 0;
  }
  float inv = 1.0f / area;
  *w0 = edgef(b, c, x, y) * inv;
  *w1 = edgef(c, a, x, y) * inv;
  *w2 = edgef(a, b, x, y) * inv;
  return *w0 >= -1e-5f && *w1 >= -1e-5f && *w2 >= -1e-5f;
}

static void draw_tri_z(cgl_image *img, float *zb, const V *a, const V *b,
                       const V *c, int use_z) {
  int minx = (int)floorf(fminf(a->x, fminf(b->x, c->x)));
  int maxx = (int)ceilf(fmaxf(a->x, fmaxf(b->x, c->x)));
  int miny = (int)floorf(fminf(a->y, fminf(b->y, c->y)));
  int maxy = (int)ceilf(fmaxf(a->y, fmaxf(b->y, c->y)));
  if (minx < 0) minx = 0;
  if (miny < 0) miny = 0;
  if (maxx >= img->w) maxx = img->w - 1;
  if (maxy >= img->h) maxy = img->h - 1;
  for (int y = miny; y <= maxy; ++y) {
    for (int x = minx; x <= maxx; ++x) {
      float w0, w1, w2;
      if (!bary(a, b, c, x + 0.5f, y + 0.5f, &w0, &w1, &w2)) {
        continue;
      }
      float z = w0 * a->z + w1 * b->z + w2 * c->z;
      int id = y * img->w + x;
      if (use_z && z >= zb[id]) {
        continue;
      }
      if (use_z) {
        zb[id] = z;
      }
      cgl_vec3 col = cgl_add3(cgl_scale3(a->col, w0),
                              cgl_add3(cgl_scale3(b->col, w1),
                                       cgl_scale3(c->col, w2)));
      cgl_image_set(img, x, y, col);
    }
  }
}

static int self_test(void) {
  cgl_image *img = cgl_image_create(8, 8);
  float zb[64];
  for (int i = 0; i < 64; ++i) zb[i] = 1e30f;
  V a = {0, 0, 0.9f, {1, 0, 0}};
  V b = {8, 0, 0.9f, {1, 0, 0}};
  V c = {0, 8, 0.9f, {1, 0, 0}};
  V d = {0, 0, 0.1f, {0, 1, 0}};
  V e = {8, 0, 0.1f, {0, 1, 0}};
  V f = {0, 8, 0.1f, {0, 1, 0}};
  draw_tri_z(img, zb, &a, &b, &c, 1);
  draw_tri_z(img, zb, &d, &e, &f, 1);
  cgl_vec3 got = cgl_image_get(img, 1, 1);
  CGL_EXPECT(got.y > 0.5f, "near green wins");
  cgl_image_free(img);
  return cgl_selftest_report("lab20-visibility");
}

static void fill_bg(cgl_image *img) {
  for (int y = 0; y < img->h; ++y) {
    for (int x = 0; x < img->w; ++x) {
      cgl_image_set(img, x, y, cgl_v3(0.08f, 0.08f, 0.1f));
    }
  }
}

static void make_tris(V tris[4][3]) {
  /* two interpenetrating quads as 2 tris each, wrong painter order vs z */
  tris[0][0] = (V){120, 60, 0.30f, {0.9f, 0.25f, 0.2f}};
  tris[0][1] = (V){420, 80, 0.55f, {0.9f, 0.25f, 0.2f}};
  tris[0][2] = (V){100, 280, 0.20f, {0.85f, 0.35f, 0.25f}};
  tris[1][0] = (V){140, 260, 0.60f, {0.2f, 0.55f, 0.95f}};
  tris[1][1] = (V){480, 240, 0.25f, {0.2f, 0.55f, 0.95f}};
  tris[1][2] = (V){300, 80, 0.45f, {0.25f, 0.65f, 0.9f}};
  tris[2][0] = (V){200, 40, 0.40f, {0.95f, 0.85f, 0.25f}};
  tris[2][1] = (V){520, 200, 0.35f, {0.9f, 0.8f, 0.3f}};
  tris[2][2] = (V){220, 300, 0.50f, {0.85f, 0.75f, 0.2f}};
  tris[3][0] = (V){80, 160, 0.70f, {0.35f, 0.85f, 0.45f}};
  tris[3][1] = (V){360, 40, 0.35f, {0.4f, 0.9f, 0.5f}};
  tris[3][2] = (V){400, 300, 0.40f, {0.3f, 0.8f, 0.4f}};
}

int main(int argc, char **argv) {
  int w = 600, h = 360;
  const char *prefix = "out/lab20";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      prefix = argv[++i];
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    }
  }
  V tris[4][3];
  make_tris(tris);
  cgl_image *zb_img = cgl_image_create(w, h);
  cgl_image *pt_img = cgl_image_create(w, h);
  cgl_image *dp_img = cgl_image_create(w, h);
  float *zb = (float *)malloc((size_t)w * h * sizeof(float));
  if (!zb_img || !pt_img || !dp_img || !zb) {
    fprintf(stderr, "lab20: allocation failed\n");
    free(zb);
    cgl_image_free(zb_img);
    cgl_image_free(pt_img);
    cgl_image_free(dp_img);
    return 1;
  }
  for (int i = 0; i < w * h; ++i) {
    zb[i] = 1e30f;
  }
  fill_bg(zb_img);
  fill_bg(pt_img);
  fill_bg(dp_img);
  /* painter: draw in array order (no depth) */
  for (int i = 0; i < 4; ++i) {
    draw_tri_z(pt_img, zb, &tris[i][0], &tris[i][1], &tris[i][2], 0);
  }
  /* zbuffer */
  for (int i = 0; i < 4; ++i) {
    draw_tri_z(zb_img, zb, &tris[i][0], &tris[i][1], &tris[i][2], 1);
  }
  /* depth map from last zbuffer state - recompute clean */
  for (int i = 0; i < w * h; ++i) zb[i] = 1e30f;
  cgl_image *tmp = cgl_image_create(w, h);
  fill_bg(tmp);
  for (int i = 0; i < 4; ++i) {
    draw_tri_z(tmp, zb, &tris[i][0], &tris[i][1], &tris[i][2], 1);
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float z = zb[y * w + x];
      float t = z > 1e29f ? 0.0f : cgl_clampf(1.0f - (z - 0.15f) / 0.6f, 0.05f, 1.0f);
      cgl_image_set(dp_img, x, y, cgl_v3(t, t, t * 1.05f));
    }
  }
  char path[256];
  int werr = 0;
  snprintf(path, sizeof path, "%s_zbuffer.ppm", prefix);
  werr |= cgl_image_write_ppm_srgb(zb_img, path) != 0;
  snprintf(path, sizeof path, "%s_painter.ppm", prefix);
  werr |= cgl_image_write_ppm_srgb(pt_img, path) != 0;
  snprintf(path, sizeof path, "%s_depth.ppm", prefix);
  werr |= cgl_image_write_ppm_srgb(dp_img, path) != 0;
  if (werr) {
    fprintf(stderr, "lab20: failed to write one or more outputs under %s\n",
            prefix);
  }
  printf("lab20: wrote %s_{zbuffer,painter,depth}.ppm\n", prefix);
  cgl_image_free(zb_img);
  cgl_image_free(pt_img);
  cgl_image_free(dp_img);
  cgl_image_free(tmp);
  free(zb);
  return werr ? 1 : 0;
}
