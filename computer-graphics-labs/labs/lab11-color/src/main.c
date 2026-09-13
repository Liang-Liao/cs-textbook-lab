#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_color.h"
#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static int self_test(void) {
  CGL_EXPECT_NEAR(cgl_linear_to_srgb1(0.0f), 0.0f, 1e-6, "srgb0");
  CGL_EXPECT_NEAR(cgl_linear_to_srgb1(1.0f), 1.0f, 1e-6, "srgb1");
  float mid = cgl_linear_to_srgb1(0.5f);
  CGL_EXPECT(mid > 0.5f && mid < 0.8f, "srgb brightens mid greys");
  CGL_EXPECT_NEAR(cgl_srgb_to_linear1(mid), 0.5f, 1e-5, "srgb invert");
  cgl_vec3 c = cgl_linear_to_srgb(cgl_v3(0.2f, 0.5f, 0.8f));
  cgl_vec3 b = cgl_srgb_to_linear(c);
  CGL_EXPECT_NEAR(b.y, 0.5f, 1e-4, "roundtrip");
  return cgl_selftest_report("lab11-color");
}

int main(int argc, char **argv) {
  int w = 512, h = 256;
  const char *out = "out/lab11_color.ppm";
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
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float u = (x + 0.5f) / w;
      float v = (y + 0.5f) / h;
      cgl_vec3 lin = cgl_v3(u, 0.5f * (1.0f - u) + 0.25f, 1.0f - u);
      if (v < 0.5f) {
        /* top: treat values as already display-referred (no extra encode in writer path).
           We write via raw ppm (no sRGB) so linear==shown. */
        cgl_image_set(img, x, y, lin);
      } else {
        cgl_image_set(img, x, y, cgl_linear_to_srgb(lin));
      }
    }
  }
  /* write raw (no second sRGB) so top is linear-as-encoded, bottom is sRGB-encoded linear */
  if (cgl_image_write_ppm(img, out, 1) != 0) {
    fprintf(stderr, "lab11: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab11: wrote %s (top linear, bottom sRGB)\n", out);
  cgl_image_free(img);
  return 0;
}
