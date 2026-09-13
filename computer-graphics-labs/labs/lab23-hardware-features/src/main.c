#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static float edgef(float ax, float ay, float bx, float by, float px, float py) {
  return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/* Z-buffered triangle fill with a polygon-offset (depth bias) toggle:
 * the fragment depth z - bias is used for both the test and the write,
 * matching what glPolygonOffset does for the depth test. */
static void fill_tri_z(cgl_image *img, float *zb, int w, int h, float x0,
                       float y0, float z0, float x1, float y1, float z1,
                       float x2, float y2, float z2, cgl_vec3 color,
                       float bias) {
  float area = edgef(x0, y0, x1, y1, x2, y2);
  if (fabsf(area) < 1e-9f) {
    return;
  }
  float minx = fminf(x0, fminf(x1, x2));
  float maxx = fmaxf(x0, fmaxf(x1, x2));
  float miny = fminf(y0, fminf(y1, y2));
  float maxy = fmaxf(y0, fmaxf(y1, y2));
  int ix0 = (int)floorf(minx), ix1 = (int)ceilf(maxx);
  int iy0 = (int)floorf(miny), iy1 = (int)ceilf(maxy);
  if (ix0 < 0) ix0 = 0;
  if (iy0 < 0) iy0 = 0;
  if (ix1 > w - 1) ix1 = w - 1;
  if (iy1 > h - 1) iy1 = h - 1;
  for (int y = iy0; y <= iy1; ++y) {
    for (int x = ix0; x <= ix1; ++x) {
      float px = x + 0.5f, py = y + 0.5f;
      float l0 = edgef(x1, y1, x2, y2, px, py) / area;
      float l1 = edgef(x2, y2, x0, y0, px, py) / area;
      float l2 = edgef(x0, y0, x1, y1, px, py) / area;
      if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) {
        continue;
      }
      float z = l0 * z0 + l1 * z1 + l2 * z2;
      int idx = y * w + x;
      if (z - bias < zb[idx]) {
        zb[idx] = z - bias;
        cgl_image_set(img, x, y, color);
      }
    }
  }
}

static void draw_offset_pair(cgl_image *img, float *zb, int w, int h, int x_off,
                             float bias) {
  for (int i = 0; i < w * h; ++i) {
    zb[i] = 1e30f;
  }
  /* coplanar "wall" + equal-z "decal": without bias the decal always loses
   * the strict depth test; with bias it lifts just in front of the wall */
  fill_tri_z(img, zb, w, h, (float)x_off + 16, 244, 0.5f, (float)x_off + 304,
             244, 0.5f, (float)x_off + 16, 316, 0.5f, cgl_v3(0.55f, 0.55f, 0.6f),
             0.0f);
  fill_tri_z(img, zb, w, h, (float)x_off + 36, 252, 0.5f, (float)x_off + 180,
             252, 0.5f, (float)x_off + 36, 304, 0.5f,
             cgl_v3(0.9f, 0.5f, 0.2f), bias);
}

static int self_test(void) {
  cgl_vec3 src = cgl_v3(1, 0, 0), dst = cgl_v3(0, 0, 1);
  float a = 0.5f;
  cgl_vec3 o = cgl_add3(cgl_scale3(src, a), cgl_scale3(dst, 1 - a));
  CGL_EXPECT_NEAR(o.x, 0.5f, 1e-5, "blend r");
  CGL_EXPECT_NEAR(o.z, 0.5f, 1e-5, "blend b");

  cgl_image *im = cgl_image_create(8, 8);
  CGL_EXPECT(im != NULL, "selftest image alloc");
  if (im) {
    float zb[64];
    cgl_vec3 ca = cgl_v3(1, 0, 0), cb = cgl_v3(0, 1, 0);
    for (int i = 0; i < 64; ++i) {
      zb[i] = 1e30f;
    }
    /* coplanar wall then equal-z decal on a tiny 8x8 canvas */
    fill_tri_z(im, zb, 8, 8, 0, 0, 0.5f, 8, 0, 0.5f, 0, 8, 0.5f, ca, 0.0f);
    fill_tri_z(im, zb, 8, 8, 2, 2, 0.5f, 6, 2, 0.5f, 2, 6, 0.5f, cb, 0.0f);
    int nb = 0;
    for (int i = 0; i < 64; ++i) {
      if (cgl_image_get(im, i % 8, i / 8).y > 0.5f) {
        nb++;
      }
    }
    CGL_EXPECT(nb == 0, "equal-z decal loses strict depth test without bias");
    for (int i = 0; i < 64; ++i) {
      cgl_image_set(im, i % 8, i / 8, cgl_v3(0, 0, 0));
      zb[i] = 1e30f;
    }
    fill_tri_z(im, zb, 8, 8, 0, 0, 0.5f, 8, 0, 0.5f, 0, 8, 0.5f, ca, 0.0f);
    fill_tri_z(im, zb, 8, 8, 2, 2, 0.5f, 6, 2, 0.5f, 2, 6, 0.5f, cb, 0.05f);
    nb = 0;
    for (int i = 0; i < 64; ++i) {
      if (cgl_image_get(im, i % 8, i / 8).y > 0.5f) {
        nb++;
      }
    }
    CGL_EXPECT(nb > 0, "decal passes depth test with polygon offset");
    cgl_image_free(im);
  }
  return cgl_selftest_report("lab23-hardware-features");
}

int main(int argc, char **argv) {
  int w = 640, h = 320;
  float bias = 0.02f;
  const char *out = "out/lab23_features.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--bias") && i + 1 < argc) {
      bias = (float)atof(argv[++i]);
    }
  }
  cgl_image *img = cgl_image_create(w, h);
  unsigned char *stencil = (unsigned char *)calloc((size_t)w * h, 1);
  float *zb = (float *)malloc(sizeof(float) * (size_t)w * h);
  if (!img || !stencil || !zb) {
    fprintf(stderr, "lab23: allocation failed\n");
    free(zb);
    free(stencil);
    cgl_image_free(img);
    return 1;
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float t = (x + 0.5f) / w;
      cgl_image_set(img, x, y, cgl_v3(t * 0.2f, 0.15f, 0.25f));
    }
  }
  /* Left half: alpha blend red over background. Right: no blend (replace). */
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float cx = x - w * 0.25f, cy = y - h * 0.5f;
      if (cx * cx / (80.0f * 80.0f) + cy * cy / (90.0f * 90.0f) <= 1.0f) {
        cgl_vec3 dst = cgl_image_get(img, x, y);
        cgl_vec3 src = cgl_v3(0.95f, 0.3f, 0.25f);
        float a = 0.55f;
        cgl_image_set(img, x, y, cgl_add3(cgl_scale3(src, a), cgl_scale3(dst, 1 - a)));
      }
      cx = x - w * 0.75f;
      if (cx * cx / (80.0f * 80.0f) + cy * cy / (90.0f * 90.0f) <= 1.0f) {
        cgl_image_set(img, x, y, cgl_v3(0.95f, 0.3f, 0.25f));
      }
    }
  }
  /* Stencil mask: the green bar is drawn only where stencil==1 (left half). */
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float cx = x - w * 0.25f, cy = y - h * 0.5f;
      stencil[y * w + x] = (cx * cx / (110.0f * 110.0f) + cy * cy / (110.0f * 110.0f) <= 1.0f);
    }
  }
  for (int y = h / 2 - 12; y < h / 2 + 12; ++y) {
    for (int x = 0; x < w / 2; ++x) {
      if (stencil[y * w + x]) {
        cgl_image_set(img, x, y, cgl_v3(0.3f, 0.9f, 0.4f));
      }
    }
  }
  /* Polygon offset: bottom strip, same coplanar wall+decal on both halves;
   * left bias=0 (decal z-fights away), right bias on (decal visible). */
  draw_offset_pair(img, zb, w, h, 0, 0.0f);
  draw_offset_pair(img, zb, w, h, w / 2, bias);
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab23: failed to write %s\n", out);
    free(zb);
    free(stencil);
    cgl_image_free(img);
    return 1;
  }
  printf("lab23: wrote %s (left blend+stencil bar, right opaque, bottom strip: offset off/on bias=%g)\n",
         out, bias);
  free(zb);
  free(stencil);
  cgl_image_free(img);
  return 0;
}
