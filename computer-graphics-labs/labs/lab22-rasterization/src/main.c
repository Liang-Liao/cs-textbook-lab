#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

typedef struct {
  float x, y, z;
  cgl_vec3 color;
} Vtx;

typedef struct {
  int i0, i1, i2;
} Tri;

static float edge(const Vtx *a, const Vtx *b, float x, float y) {
  return (x - a->x) * (b->y - a->y) - (y - a->y) * (b->x - a->x);
}

static int barycentric(const Vtx *a, const Vtx *b, const Vtx *c, float x,
                       float y, float *w0, float *w1, float *w2) {
  float area = edge(a, b, c->x, c->y);
  if (fabsf(area) < 1e-12f) {
    return 0;
  }
  float inv = 1.0f / area;
  *w0 = edge(b, c, x, y) * inv;
  *w1 = edge(c, a, x, y) * inv;
  *w2 = edge(a, b, x, y) * inv;
  /* allow tiny outside epsilon for edge cases */
  const float eps = -1e-5f;
  return *w0 >= eps && *w1 >= eps && *w2 >= eps;
}

static void draw_tri(cgl_image *img, float *zbuf, const Vtx *a, const Vtx *b,
                     const Vtx *c) {
  int w = img->w, h = img->h;
  int minx = (int)floorf(fminf(a->x, fminf(b->x, c->x)));
  int maxx = (int)ceilf(fmaxf(a->x, fmaxf(b->x, c->x)));
  int miny = (int)floorf(fminf(a->y, fminf(b->y, c->y)));
  int maxy = (int)ceilf(fmaxf(a->y, fmaxf(b->y, c->y)));
  if (minx < 0) minx = 0;
  if (miny < 0) miny = 0;
  if (maxx >= w) maxx = w - 1;
  if (maxy >= h) maxy = h - 1;
  for (int y = miny; y <= maxy; ++y) {
    for (int x = minx; x <= maxx; ++x) {
      float w0, w1, w2;
      if (!barycentric(a, b, c, x + 0.5f, y + 0.5f, &w0, &w1, &w2)) {
        continue;
      }
      float z = w0 * a->z + w1 * b->z + w2 * c->z;
      int idx = y * w + x;
      if (z >= zbuf[idx]) {
        continue;
      }
      zbuf[idx] = z;
      cgl_vec3 col = cgl_add3(
          cgl_scale3(a->color, w0),
          cgl_add3(cgl_scale3(b->color, w1), cgl_scale3(c->color, w2)));
      cgl_image_set(img, x, y, col);
    }
  }
}

static int self_test(void) {
  Vtx a = {0, 0, 0, {1, 0, 0}};
  Vtx b = {10, 0, 0, {0, 1, 0}};
  Vtx c = {0, 10, 0, {0, 0, 1}};
  float w0, w1, w2;
  CGL_EXPECT(barycentric(&a, &b, &c, 1, 1, &w0, &w1, &w2), "inside");
  CGL_EXPECT_NEAR(w0 + w1 + w2, 1.0f, 1e-5, "weights sum");
  CGL_EXPECT(!barycentric(&a, &b, &c, 20, 20, &w0, &w1, &w2), "outside");
  Vtx d = {1, 1, 0.5f, {1, 1, 1}};
  CGL_EXPECT(barycentric(&a, &b, &c, d.x, d.y, &w0, &w1, &w2), "d inside");
  float z = w0 * a.z + w1 * b.z + w2 * c.z;
  CGL_EXPECT_NEAR(z, 0.0f, 1e-5, "z at plane");

  /* zbuffer: nearer pixel wins */
  cgl_image *img = cgl_image_create(4, 4);
  CGL_EXPECT(img != NULL, "img");
  float zbuf[16];
  for (int i = 0; i < 16; ++i) {
    zbuf[i] = 1e30f;
  }
  Vtx q0 = {0, 0, 0.8f, {1, 0, 0}};
  Vtx q1 = {4, 0, 0.8f, {1, 0, 0}};
  Vtx q2 = {0, 4, 0.8f, {1, 0, 0}};
  draw_tri(img, zbuf, &q0, &q1, &q2);
  cgl_vec3 got = cgl_image_get(img, 1, 1);
  CGL_EXPECT(got.x > 0.5f, "far red drawn");
  Vtx p0 = {0, 0, 0.2f, {0, 1, 0}};
  Vtx p1 = {4, 0, 0.2f, {0, 1, 0}};
  Vtx p2 = {0, 4, 0.2f, {0, 1, 0}};
  draw_tri(img, zbuf, &p0, &p1, &p2);
  got = cgl_image_get(img, 1, 1);
  CGL_EXPECT(got.y > 0.5f, "near green overwrites");
  cgl_image_free(img);
  return cgl_selftest_report("lab22-rasterization");
}

int main(int argc, char **argv) {
  int w = 640, h = 360;
  const char *out = "out/lab22_raster.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    }
  }
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  float *zbuf = (float *)malloc((size_t)w * (size_t)h * sizeof(float));
  if (!zbuf) {
    cgl_image_free(img);
    return 1;
  }
  for (int i = 0; i < w * h; ++i) {
    zbuf[i] = 1e30f;
    img->rgb[i * 3 + 0] = 0.08f;
    img->rgb[i * 3 + 1] = 0.08f;
    img->rgb[i * 3 + 2] = 0.1f;
  }
  /* Simple perspective-ish grid of triangles */
  Vtx vs[5][5];
  for (int j = 0; j < 5; ++j) {
    for (int i = 0; i < 5; ++i) {
      float x = -1.2f + 0.6f * i;
      float y = -0.8f + 0.4f * j;
      float z = 0.8f * sinf(x * 1.3f) * cosf(y * 1.7f);
      float sx = (x * 0.35f + 0.5f) * (float)w;
      float sy = (0.5f - y * 0.45f) * (float)h;
      float depth = 0.4f - z * 0.15f + 0.05f * j;
      vs[j][i] = (Vtx){sx, sy, depth,
                       {0.2f + 0.15f * i, 0.3f + 0.1f * j, 0.85f - 0.1f * z}};
    }
  }
  Tri tris[32];
  int nt = 0;
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      tris[nt++] = (Tri){j * 5 + i, j * 5 + i + 1, (j + 1) * 5 + i};
      tris[nt++] = (Tri){j * 5 + i + 1, (j + 1) * 5 + i + 1, (j + 1) * 5 + i};
    }
  }
  Vtx flat[25];
  for (int i = 0; i < 25; ++i) {
    flat[i] = vs[i / 5][i % 5];
  }
  /* draw far first then near: also mix order to show z-buffer */
  for (int i = 0; i < nt; ++i) {
    draw_tri(img, zbuf, &flat[tris[i].i0], &flat[tris[i].i1],
             &flat[tris[i].i2]);
  }
  /* foreground triangle overlapping */
  Vtx fg0 = {0.55f * w, 0.2f * h, 0.05f, {1.0f, 0.35f, 0.15f}};
  Vtx fg1 = {0.9f * w, 0.75f * h, 0.05f, {0.9f, 0.8f, 0.2f}};
  Vtx fg2 = {0.25f * w, 0.8f * h, 0.05f, {0.2f, 0.85f, 0.7f}};
  draw_tri(img, zbuf, &fg0, &fg1, &fg2);
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab22: failed to write %s\n", out);
    free(zbuf);
    cgl_image_free(img);
    return 1;
  }
  printf("lab22: wrote %s (%d tris)\n", out, nt + 1);
  free(zbuf);
  cgl_image_free(img);
  return 0;
}
