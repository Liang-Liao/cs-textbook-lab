#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static int self_test(void) {
  cgl_vec3 x = cgl_v3(1, 0, 0), y = cgl_v3(0, 1, 0);
  CGL_EXPECT_NEAR(cgl_dot3(x, y), 0.0f, 1e-6, "dot ortho");
  cgl_vec3 c = cgl_cross3(x, y);
  CGL_EXPECT_NEAR(c.x, 0, 1e-6, "cross x");
  CGL_EXPECT_NEAR(c.y, 0, 1e-6, "cross y");
  CGL_EXPECT_NEAR(c.z, 1, 1e-6, "cross z");

  cgl_vec3 v = cgl_v3(3, 4, 0);
  cgl_vec3 u = cgl_normalize3(v);
  CGL_EXPECT_NEAR(cgl_len3(u), 1.0f, 1e-5, "unit length");
  float proj = cgl_dot3(v, x);
  CGL_EXPECT_NEAR(proj, 3.0f, 1e-5, "project onto x");
  cgl_vec3 along = cgl_scale3(x, proj);
  cgl_vec3 perp = cgl_sub3(v, along);
  CGL_EXPECT_NEAR(cgl_dot3(perp, x), 0.0f, 1e-5, "perp residual");

  cgl_vec3 i = cgl_normalize3(cgl_v3(1, -1, 0));
  cgl_vec3 r = cgl_reflect3(i, y);
  CGL_EXPECT_NEAR(r.y, -i.y, 1e-5, "reflect flips normal component");
  CGL_EXPECT_NEAR(r.x, i.x, 1e-5, "reflect keeps tangent");

  cgl_mat4 T = cgl_translate(cgl_v3(1, 2, 3));
  cgl_vec3 p = cgl_mat4_mul_point(&T, cgl_v3(0, 0, 0));
  CGL_EXPECT_NEAR(p.x, 1, 1e-5, "translate x");
  CGL_EXPECT_NEAR(p.y, 2, 1e-5, "translate y");
  CGL_EXPECT_NEAR(p.z, 3, 1e-5, "translate z");
  return cgl_selftest_report("lab04-linear-algebra");
}

int main(int argc, char **argv) {
  int w = 512, h = 512;
  const char *out = "out/lab04_vectors.ppm";
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
  /* Vector field: rotate 2D positions around origin, color encodes angle. */
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float u = (x + 0.5f) / (float)w * 2.0f - 1.0f;
      float v = 1.0f - (y + 0.5f) / (float)h * 2.0f;
      /* tangent field of (u,v) rotated 90 deg in plane: d = (-v, u) */
      cgl_vec2 d = cgl_v2(-v, u);
      float len = sqrtf(d.x * d.x + d.y * d.y);
      if (len < 1e-5f) {
        d = cgl_v2(1, 0);
        len = 1.0f;
      }
      d.x /= len;
      d.y /= len;
      float ang = atan2f(d.y, d.x);
      float t = (ang + CGL_PI) / (2.0f * CGL_PI);
      cgl_vec3 col = cgl_v3(0.15f + 0.85f * t, 0.4f + 0.3f * (1.0f - t),
                            0.2f + 0.8f * (1.0f - t));
      /* draw axis cross in bright white */
      if (fabsf(u) < 0.003f || fabsf(v) < 0.003f) {
        col = cgl_v3(1, 1, 1);
      }
      cgl_image_set(img, x, y, col);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab04: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab04: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
