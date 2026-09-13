#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static cgl_vec3 g_checker(float u, float v) {
  int x = (int)floorf(u * 8.0f);
  int y = (int)floorf(v * 8.0f);
  return ((x + y) & 1) ? cgl_v3(0.9f, 0.9f, 0.9f) : cgl_v3(0.1f, 0.1f, 0.15f);
}

static cgl_vec3 g_rings(float u, float v) {
  float dx = u - 0.5f, dy = v - 0.5f;
  float r = sqrtf(dx * dx + dy * dy);
  float t = 0.5f + 0.5f * cosf(r * 40.0f);
  return cgl_v3(t, 0.35f + 0.3f * t, 1.0f - t);
}

static cgl_vec3 g_interference(float u, float v) {
  float a = sinf(u * 25.0f) * cosf(v * 18.0f);
  float b = sinf((u + v) * 12.0f);
  float t = 0.5f + 0.25f * (a + b);
  return cgl_v3(t, 1.0f - t, 0.35f + 0.3f * a);
}

static cgl_vec3 g_radial(float u, float v) {
  float dx = u - 0.5f, dy = v - 0.5f;
  float r = cgl_clampf(sqrtf(dx * dx + dy * dy) * 2.0f, 0.0f, 1.0f);
  return cgl_v3(1.0f - r, 0.2f + 0.3f * r, r);
}

static cgl_vec3 sample_pattern(float u, float v) {
  int cell = 0;
  float lu = u, lv = v;
  if (u >= 0.5f) {
    cell += 1;
    lu = (u - 0.5f) * 2.0f;
  } else {
    lu = u * 2.0f;
  }
  if (v >= 0.5f) {
    cell += 2;
    lv = (v - 0.5f) * 2.0f;
  } else {
    lv = v * 2.0f;
  }
  switch (cell) {
  case 0:
    return g_checker(lu, lv);
  case 1:
    return g_rings(lu, lv);
  case 2:
    return g_interference(lu, lv);
  default:
    return g_radial(lu, lv);
  }
}

static int self_test(void) {
  cgl_vec3 a = g_checker(0.0f, 0.0f);
  cgl_vec3 b = g_checker(0.2f, 0.0f);
  CGL_EXPECT(a.x != b.x, "checker changes with u");
  cgl_vec3 r0 = g_radial(0.5f, 0.5f);
  CGL_EXPECT_NEAR(r0.x, 1.0f, 1e-5, "radial center red");
  CGL_EXPECT_NEAR(cgl_len3(cgl_normalize3(cgl_v3(3, 4, 0))), 1.0f, 1e-5,
                  "normalize");
  return cgl_selftest_report("lab01-introduction");
}

int main(int argc, char **argv) {
  int w = 512, h = 512;
  const char *out = "out/lab01_patterns.ppm";
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
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float u = (x + 0.5f) / (float)w;
      float v = (y + 0.5f) / (float)h;
      cgl_image_set(img, x, y, sample_pattern(u, v));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab01: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab01: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
