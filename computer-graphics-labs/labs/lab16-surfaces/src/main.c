#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

/* 4x4 control net, row-major in u then v */
static cgl_vec3 kP[4][4];

static cgl_vec3 eval_patch(float u, float v) {
  float bu[4] = {(1 - u) * (1 - u) * (1 - u), 3 * (1 - u) * (1 - u) * u,
                 3 * (1 - u) * u * u, u * u * u};
  float bv[4] = {(1 - v) * (1 - v) * (1 - v), 3 * (1 - v) * (1 - v) * v,
                 3 * (1 - v) * v * v, v * v * v};
  cgl_vec3 s = cgl_v3(0, 0, 0);
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      s = cgl_add3(s, cgl_scale3(kP[i][j], bu[i] * bv[j]));
    }
  }
  return s;
}

static cgl_vec3 patch_normal(float u, float v) {
  float e = 1e-3f;
  cgl_vec3 du = cgl_sub3(eval_patch(u + e, v), eval_patch(u - e, v));
  cgl_vec3 dv = cgl_sub3(eval_patch(u, v + e), eval_patch(u, v - e));
  return cgl_normalize3(cgl_cross3(du, dv));
}

static void init_patch(void) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float x = -1.5f + i;
      float z = -1.5f + j;
      float y = 0.55f * cosf(x * 0.9f) * cosf(z * 0.9f);
      if (i == 1 || j == 1 || i == 2 || j == 2) {
        y += 0.15f;
      }
      kP[i][j] = cgl_v3(x, y, z);
    }
  }
}

static int self_test(void) {
  init_patch();
  cgl_vec3 a = eval_patch(0, 0);
  cgl_vec3 b = eval_patch(1, 1);
  CGL_EXPECT_NEAR(a.x, kP[0][0].x, 1e-4, "corner00");
  CGL_EXPECT_NEAR(b.x, kP[3][3].x, 1e-4, "corner11");
  cgl_vec3 n = patch_normal(0.5f, 0.5f);
  CGL_EXPECT_NEAR(cgl_len3(n), 1.0f, 1e-3, "unit normal");
  return cgl_selftest_report("lab16-surfaces");
}

int main(int argc, char **argv) {
  int w = 480, h = 320;
  const char *out = "out/lab16_surfaces.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    }
  }
  init_patch();
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  cgl_vec3 eye = cgl_v3(0.2f, 2.8f, 3.8f);
  cgl_vec3 fwd = cgl_normalize3(cgl_sub3(cgl_v3(0, 0, 0), eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(fwd, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, fwd);
  float half_h = tanf(40 * CGL_PI / 180 * 0.5f);
  float half_w = half_h * ((float)w / h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sx = (2.0f * (x + 0.5f) / w - 1) * half_w;
      float sy = (1 - 2.0f * (y + 0.5f) / h) * half_h;
      cgl_vec3 rd = cgl_normalize3(
          cgl_add3(cgl_add3(fwd, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
      /* dense UV sample intersection (educational, not robust) */
      float best = 1e30f;
      cgl_vec3 bn;
      int hit = 0;
      for (int j = 0; j <= 48; ++j) {
        for (int i = 0; i <= 48; ++i) {
          float u = i / 48.0f, v = j / 48.0f;
          cgl_vec3 p = eval_patch(u, v);
          cgl_vec3 oc = cgl_sub3(eye, p);
          float t = cgl_dot3(oc, rd) / cgl_dot3(rd, rd);
          if (t < 1e-3f) {
            continue;
          }
          cgl_vec3 q = cgl_add3(eye, cgl_scale3(rd, t));
          float dist = cgl_len3(cgl_sub3(q, p));
          if (dist < 0.06f && t < best) {
            best = t;
            bn = patch_normal(u, v);
            hit = 1;
          }
        }
      }
      cgl_vec3 col;
      if (!hit) {
        col = cgl_lerp3(cgl_v3(0.1f, 0.1f, 0.14f), cgl_v3(0.2f, 0.3f, 0.45f),
                        0.5f * (rd.y + 1));
      } else {
        float nd = cgl_clampf(cgl_dot3(bn, cgl_normalize3(cgl_v3(0.4f, 0.8f, 0.5f))), 0, 1);
        col = cgl_add3(cgl_scale3(cgl_v3(0.2f, 0.55f, 0.85f), 0.12f),
                       cgl_scale3(cgl_v3(0.9f, 0.75f, 0.45f), nd));
      }
      cgl_image_set(img, x, y, col);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab16: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab16: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
