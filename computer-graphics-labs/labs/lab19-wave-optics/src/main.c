#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static float sinc(float x) {
  if (fabsf(x) < 1e-6f) {
    return 1.0f;
  }
  return sinf(x) / x;
}

/* Intensity of double-slit (approx): I = I0 * sinc^2(beta) * cos^2(delta) */
static float double_slit(float u, float slit_w, float slit_sep) {
  float beta = CGL_PI * slit_w * u;
  float delta = CGL_PI * slit_sep * u;
  float a = sinc(beta);
  return a * a * cosf(delta) * cosf(delta);
}

static int self_test(void) {
  CGL_EXPECT_NEAR(double_slit(0.0f, 0.2f, 0.8f), 1.0f, 1e-5, "center max");
  CGL_EXPECT(double_slit(1.5f, 0.2f, 0.8f) < 0.5f, "off-center dimmer");
  return cgl_selftest_report("lab19-wave-optics");
}

int main(int argc, char **argv) {
  int w = 512, h = 256;
  const char *out = "out/lab19_diffraction.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    }
  }
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float u = ((x + 0.5f) / w - 0.5f) * 12.0f;
      float v = ((y + 0.5f) / h - 0.5f) * 8.0f;
      float r2 = u * u + v * v;
      float I = double_slit(sqrtf(r2) * 0.35f, 0.25f, 1.1f);
      I = cgl_clampf(I, 0, 1);
      cgl_image_set(img, x, y, cgl_v3(I, I * 0.85f, I * 0.55f + 0.05f));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab19: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab19: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
