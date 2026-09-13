#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define TEX_N 64

static void make_checker(cgl_image *tex) {
  for (int y = 0; y < tex->h; ++y) {
    for (int x = 0; x < tex->w; ++x) {
      int cx = x / 8, cy = y / 8;
      float a = ((cx + cy) & 1) ? 0.95f : 0.15f;
      float b = 0.3f + 0.5f * ((x + y) % 17) / 16.0f;
      cgl_image_set(tex, x, y, cgl_v3(a, b * 0.4f + 0.2f, 1.0f - a * 0.5f));
    }
  }
}

static cgl_vec3 sample_nearest(const cgl_image *tex, float u, float v) {
  u = u - floorf(u);
  v = v - floorf(v);
  int x = (int)(u * tex->w);
  int y = (int)(v * tex->h);
  if (x >= tex->w) x = tex->w - 1;
  if (y >= tex->h) y = tex->h - 1;
  return cgl_image_get(tex, x, y);
}

static cgl_vec3 sample_bilinear(const cgl_image *tex, float u, float v) {
  u = u - floorf(u);
  v = v - floorf(v);
  float x = u * tex->w - 0.5f;
  float y = v * tex->h - 0.5f;
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  float fx = x - x0;
  float fy = y - y0;
  cgl_vec3 c00 = sample_nearest(tex, (x0 + 0.5f) / tex->w, (y0 + 0.5f) / tex->h);
  cgl_vec3 c10 = sample_nearest(tex, (x0 + 1.5f) / tex->w, (y0 + 0.5f) / tex->h);
  cgl_vec3 c01 = sample_nearest(tex, (x0 + 0.5f) / tex->w, (y0 + 1.5f) / tex->h);
  cgl_vec3 c11 = sample_nearest(tex, (x0 + 1.5f) / tex->w, (y0 + 1.5f) / tex->h);
  cgl_vec3 a = cgl_lerp3(c00, c10, fx);
  cgl_vec3 b = cgl_lerp3(c01, c11, fx);
  return cgl_lerp3(a, b, fy);
}

static void sphere_uv(cgl_vec3 n, float *u, float *v) {
  *u = 0.5f + atan2f(n.z, n.x) / (2.0f * CGL_PI);
  *v = 0.5f - asinf(cgl_clampf(n.y, -1.0f, 1.0f)) / CGL_PI;
}

static int self_test(void) {
  cgl_image *tex = cgl_image_create(4, 4);
  cgl_image_fill(tex, cgl_v3(0, 0, 0));
  cgl_image_set(tex, 0, 0, cgl_v3(1, 0, 0));
  cgl_image_set(tex, 1, 0, cgl_v3(0, 1, 0));
  cgl_image_set(tex, 0, 1, cgl_v3(0, 0, 1));
  cgl_image_set(tex, 1, 1, cgl_v3(1, 1, 1));
  float u, v;
  sphere_uv(cgl_v3(0, 1, 0), &u, &v);
  CGL_EXPECT_NEAR(v, 0.0f, 1e-4, "north pole v");
  sphere_uv(cgl_v3(0, -1, 0), &u, &v);
  CGL_EXPECT_NEAR(v, 1.0f, 1e-4, "south pole v");
  cgl_vec3 c = sample_bilinear(tex, 0.5f / 4, 0.5f / 4);
  CGL_EXPECT_NEAR(c.x, 1.0f, 1e-4, "bilinear at texel00");
  cgl_image_free(tex);
  return cgl_selftest_report("lab07-texture-mapping");
}

static void render_half(cgl_image *img, int x0, int x1, const cgl_image *tex,
                        int bilinear) {
  int h = img->h;
  cgl_vec3 eye = cgl_v3(0, 0, 3);
  for (int y = 0; y < h; ++y) {
    for (int x = x0; x < x1; ++x) {
      /* map each half to independent NDC in that half */
      float halfw = (float)(x1 - x0) / (float)h;
      float sx = ((x - x0 + 0.5f) / (float)(x1 - x0) * 2.0f - 1.0f) * halfw;
      float sy = (1.0f - 2.0f * (y + 0.5f) / h);
      cgl_vec3 rd = cgl_normalize3(cgl_v3(sx, sy, -1));
      cgl_vec3 oc = cgl_sub3(eye, cgl_v3(0, 0, 0));
      float a = cgl_dot3(rd, rd);
      float b = 2 * cgl_dot3(oc, rd);
      float c = cgl_dot3(oc, oc) - 1.0f;
      float disc = b * b - 4 * a * c;
      cgl_vec3 col;
      if (disc < 0) {
        col = cgl_v3(0.1f, 0.1f, 0.12f);
      } else {
        float t = (-b - sqrtf(disc)) / (2 * a);
        cgl_vec3 p = cgl_add3(eye, cgl_scale3(rd, t));
        cgl_vec3 n = p; /* unit sphere */
        float u, v;
        sphere_uv(n, &u, &v);
        cgl_vec3 tc = bilinear ? sample_bilinear(tex, u * 4, v * 2)
                               : sample_nearest(tex, u * 4, v * 2);
        float ndotl = cgl_clampf(cgl_dot3(n, cgl_normalize3(cgl_v3(0.4f, 0.7f, 0.6f))), 0, 1);
        col = cgl_add3(cgl_scale3(tc, 0.15f), cgl_scale3(tc, ndotl));
      }
      cgl_image_set(img, x, y, col);
    }
  }
}

int main(int argc, char **argv) {
  int w = 640, h = 320;
  const char *out = "out/lab07_texture.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    }
  }
  cgl_image *tex = cgl_image_create(TEX_N, TEX_N);
  cgl_image *img = cgl_image_create(w, h);
  if (!tex || !img) {
    fprintf(stderr, "lab07: image create failed\n");
    cgl_image_free(tex);
    cgl_image_free(img);
    return 1;
  }
  make_checker(tex);
  render_half(img, 0, w / 2, tex, 0);
  render_half(img, w / 2, w, tex, 1);
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab07: failed to write %s\n", out);
    cgl_image_free(img);
    cgl_image_free(tex);
    return 1;
  }
  printf("lab07: wrote %s (left nearest, right bilinear)\n", out);
  cgl_image_free(img);
  cgl_image_free(tex);
  return 0;
}
