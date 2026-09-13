#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_color.h"
#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static int self_test(void) {
  cgl_image *img = cgl_image_create(4, 4);
  CGL_EXPECT(img != NULL, "image create");
  if (img) {
    cgl_image_set(img, 1, 2, cgl_v3(0.25f, 0.5f, 0.75f));
    cgl_vec3 c = cgl_image_get(img, 1, 2);
    CGL_EXPECT_NEAR(c.x, 0.25f, 1e-6, "get r");
    CGL_EXPECT_NEAR(c.y, 0.5f, 1e-6, "get g");
    CGL_EXPECT_NEAR(c.z, 0.75f, 1e-6, "get b");
    cgl_image_free(img);
  }
  CGL_EXPECT_NEAR(cgl_linear_to_srgb1(0.0f), 0.0f, 1e-6, "srgb 0");
  CGL_EXPECT_NEAR(cgl_linear_to_srgb1(1.0f), 1.0f, 1e-6, "srgb 1");
  CGL_EXPECT_NEAR(cgl_dot3(cgl_v3(1, 0, 0), cgl_v3(0, 1, 0)), 0.0f, 1e-6,
                  "dot ortho");
  return cgl_selftest_report("lab00-toolchain-smoke");
}

static cgl_vec3 sample_fn(float u, float v) {
  return cgl_v3(u, v, 1.0f - 0.5f * (u + v));
}

int main(int argc, char **argv) {
  int w = 256, h = 256;
  const char *out = "out/lab00_gradient.ppm";
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
  if (w <= 0 || h <= 0) {
    fprintf(stderr, "invalid size\n");
    return 1;
  }
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    fprintf(stderr, "image create failed\n");
    return 1;
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float u = (x + 0.5f) / (float)w;
      float v = (y + 0.5f) / (float)h;
      cgl_image_set(img, x, y, sample_fn(u, v));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab00: wrote %s (%dx%d)\n", out, w, h);
  cgl_image_free(img);
  return 0;
}
