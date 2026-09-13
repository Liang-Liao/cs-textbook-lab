#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

/* GGX D term (isotropic) */
static float ggx_d(float ndoth, float alpha) {
  float a2 = alpha * alpha;
  float d = ndoth * ndoth * (a2 - 1.0f) + 1.0f;
  return a2 / (CGL_PI * d * d + 1e-8f);
}

static float smith_g1(float ndotv, float alpha) {
  float a2 = alpha * alpha;
  return 2.0f * ndotv / (ndotv + sqrtf(a2 + (1 - a2) * ndotv * ndotv) + 1e-8f);
}

static int hit_sphere(cgl_vec3 ro, cgl_vec3 rd, cgl_vec3 c, float r, float *t,
                      cgl_vec3 *n) {
  cgl_vec3 oc = cgl_sub3(ro, c);
  float a = cgl_dot3(rd, rd);
  float b = 2 * cgl_dot3(oc, rd);
  float cc = cgl_dot3(oc, oc) - r * r;
  float disc = b * b - 4 * a * cc;
  if (disc < 0) {
    return 0;
  }
  float sq = sqrtf(disc);
  float t0 = (-b - sq) / (2 * a);
  float t1 = (-b + sq) / (2 * a);
  float tt = t0 > 1e-3f ? t0 : t1;
  if (tt < 1e-3f) {
    return 0;
  }
  cgl_vec3 p = cgl_add3(ro, cgl_scale3(rd, tt));
  *t = tt;
  *n = cgl_normalize3(cgl_sub3(p, c));
  return 1;
}

static int self_test(void) {
  CGL_EXPECT(ggx_d(1.0f, 0.2f) > ggx_d(1.0f, 0.8f), "sharper peak for low alpha");
  CGL_EXPECT_NEAR(smith_g1(1.0f, 0.5f), 1.0f, 0.05f, "G1 ~1 at head-on ndotv=1");
  return cgl_selftest_report("lab18-materials");
}

int main(int argc, char **argv) {
  int w = 720, h = 220;
  const char *out = "out/lab18_materials.ppm";
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
  cgl_vec3 eye = cgl_v3(0, 0.4f, 5.0f);
  cgl_vec3 centers[3] = {cgl_v3(-1.6f, 0, 0), cgl_v3(0, 0, 0),
                         cgl_v3(1.6f, 0, 0)};
  /* 0: diffuse, 1: microfacet metal alpha 0.12, 2: microfacet metal alpha 0.45 */
  float rough[3] = {1.0f, 0.12f, 0.45f};
  cgl_vec3 albedo[3] = {{0.7f, 0.25f, 0.2f}, {0.9f, 0.75f, 0.35f},
                        {0.85f, 0.7f, 0.3f}};
  cgl_vec3 light_pos = cgl_v3(2.0f, 3.0f, 2.5f);
  cgl_vec3 light_col = cgl_scale3(cgl_v3(1.2f, 1.15f, 1.05f), 8.0f);
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
      float best = 1e30f;
      int bi = -1;
      cgl_vec3 bn;
      for (int i = 0; i < 3; ++i) {
        float t;
        cgl_vec3 n;
        if (hit_sphere(eye, rd, centers[i], 0.9f, &t, &n) && t < best) {
          best = t;
          bi = i;
          bn = n;
        }
      }
      float t;
      if (fabsf(rd.y) > 1e-8f) {
        t = (-1.0f - eye.y) / rd.y;
        if (t > 1e-3f && t < best) {
          best = t;
          bi = 50;
          bn = cgl_v3(0, 1, 0);
        }
      }
      cgl_vec3 col = cgl_v3(0.08f, 0.09f, 0.12f);
      if (bi >= 0) {
        cgl_vec3 p = cgl_add3(eye, cgl_scale3(rd, best));
        cgl_vec3 V = cgl_neg3(rd);
        cgl_vec3 L = cgl_sub3(light_pos, p);
        float dist2 = cgl_len2_3(L);
        cgl_vec3 wi = cgl_normalize3(L);
        cgl_vec3 H = cgl_normalize3(cgl_add3(wi, V));
        float ndotl = cgl_clampf(cgl_dot3(bn, wi), 0, 1);
        float ndotv = cgl_clampf(cgl_dot3(bn, V), 1e-4f, 1);
        float ndoth = cgl_clampf(cgl_dot3(bn, H), 0, 1);
        float att = 1.0f / (1.0f + 0.04f * dist2);
        if (bi == 50) {
          int cx = (int)floorf(p.x * 0.7f);
          int cz = (int)floorf(p.z * 0.7f);
          float s = ((cx + cz) & 1) ? 0.45f : 0.65f;
          cgl_vec3 alb = cgl_v3(s, s, s * 0.98f);
          col = cgl_add3(cgl_scale3(alb, 0.08f),
                         cgl_scale3(cgl_mul3(alb, light_col), ndotl * att * 0.15f));
        } else {
          cgl_vec3 alb = albedo[bi];
          if (bi == 0) {
            col = cgl_add3(cgl_scale3(alb, 0.1f),
                           cgl_scale3(cgl_mul3(alb, light_col), ndotl * att * 0.12f));
          } else {
            float a = rough[bi];
            float D = ggx_d(ndoth, a);
            float G = smith_g1(ndotv, a) * smith_g1(ndotl, a);
            float spec = D * G / (4.0f * ndotv * ndotl + 1e-6f);
            col = cgl_add3(cgl_scale3(alb, 0.05f),
                           cgl_scale3(cgl_mul3(alb, light_col),
                                      spec * ndotl * att * 0.08f));
          }
        }
      }
      cgl_image_set(img, x, y, col);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab18: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab18: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
