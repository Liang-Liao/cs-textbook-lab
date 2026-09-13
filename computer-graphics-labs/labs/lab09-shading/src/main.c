#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

typedef enum {
  SHADE_AMBIENT,
  SHADE_LAMBERT,
  SHADE_PHONG,
  SHADE_BLINN,
  SHADE_MIXED
} ShadeModel;

static const cgl_vec3 kLightPos = {-2.5f, 4.0f, 2.5f};
static const cgl_vec3 kLightColor = {1.4f, 1.35f, 1.25f};
static const cgl_vec3 kEye = {0.0f, 1.5f, 5.0f};

static cgl_vec3 shade(ShadeModel model, cgl_vec3 p, cgl_vec3 n, cgl_vec3 albedo,
                      float shininess, float kspec) {
  cgl_vec3 amb = cgl_scale3(albedo, 0.08f);
  cgl_vec3 L = cgl_sub3(kLightPos, p);
  float dist = cgl_len3(L);
  L = cgl_scale3(L, 1.0f / dist);
  float ndotl = cgl_clampf(cgl_dot3(n, L), 0.0f, 1.0f);
  float atten = 1.0f / (0.4f + 0.12f * dist + 0.04f * dist * dist);
  cgl_vec3 diff = cgl_scale3(cgl_mul3(albedo, kLightColor), ndotl * atten);

  if (model == SHADE_AMBIENT) {
    return amb;
  }
  if (model == SHADE_LAMBERT) {
    return cgl_add3(amb, diff);
  }

  cgl_vec3 V = cgl_normalize3(cgl_sub3(kEye, p));
  float spec = 0.0f;
  if (model == SHADE_PHONG || model == SHADE_MIXED) {
    cgl_vec3 R = cgl_reflect3(cgl_neg3(L), n);
    float rdotv = cgl_clampf(cgl_dot3(R, V), 0.0f, 1.0f);
    spec = powf(rdotv, shininess);
  }
  if (model == SHADE_BLINN) {
    cgl_vec3 H = cgl_normalize3(cgl_add3(L, V));
    float ndoth = cgl_clampf(cgl_dot3(n, H), 0.0f, 1.0f);
    spec = powf(ndoth, shininess);
  }
  if (model == SHADE_MIXED) {
    cgl_vec3 R = cgl_reflect3(cgl_neg3(L), n);
    float phong = powf(cgl_clampf(cgl_dot3(R, V), 0.0f, 1.0f), shininess);
    cgl_vec3 H = cgl_normalize3(cgl_add3(L, V));
    float blinn = powf(cgl_clampf(cgl_dot3(n, H), 0.0f, 1.0f), shininess * 1.2f);
    spec = 0.45f * phong + 0.55f * blinn;
  }
  cgl_vec3 specc = cgl_scale3(kLightColor, spec * kspec * atten);
  return cgl_add3(amb, cgl_add3(diff, specc));
}

static int hit_sphere(cgl_vec3 ro, cgl_vec3 rd, cgl_vec3 c, float r, float *t,
                      cgl_vec3 *n) {
  cgl_vec3 oc = cgl_sub3(ro, c);
  float a = cgl_dot3(rd, rd);
  float b = 2.0f * cgl_dot3(oc, rd);
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
  cgl_vec3 n = cgl_v3(0, 1, 0);
  cgl_vec3 p = cgl_v3(0, 0, 0);
  cgl_vec3 albedo = cgl_v3(0.8f, 0.2f, 0.2f);
  cgl_vec3 amb = shade(SHADE_AMBIENT, p, n, albedo, 32.0f, 0.5f);
  CGL_EXPECT(amb.x > 0 && amb.y > 0, "ambient nonzero");
  cgl_vec3 lam = shade(SHADE_LAMBERT, p, n, albedo, 32.0f, 0.5f);
  CGL_EXPECT(cgl_len3(lam) > cgl_len3(amb), "lambert brighter than ambient");
  /* normal facing away from light => diffuse ~ ambient only */
  cgl_vec3 away = shade(SHADE_LAMBERT, p, cgl_v3(0, -1, 0), albedo, 32, 0.5f);
  CGL_EXPECT_NEAR(cgl_len3(cgl_sub3(away, amb)), 0.0f, 1e-5, "backface diffuse");
  return cgl_selftest_report("lab09-shading");
}

int main(int argc, char **argv) {
  int w = 640, h = 240;
  const char *out = "out/lab09_shading.ppm";
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
  cgl_vec3 centers[5];
  ShadeModel models[5] = {SHADE_AMBIENT, SHADE_LAMBERT, SHADE_PHONG,
                          SHADE_BLINN, SHADE_MIXED};
  cgl_vec3 albedos[5] = {
      {0.75f, 0.75f, 0.75f}, {0.85f, 0.25f, 0.2f}, {0.2f, 0.45f, 0.85f},
      {0.2f, 0.75f, 0.35f}, {0.9f, 0.75f, 0.35f}};
  for (int i = 0; i < 5; ++i) {
    centers[i] = cgl_v3((float)(i - 2) * 1.25f, 0.0f, 0.0f);
  }
  cgl_vec3 forward = cgl_normalize3(cgl_sub3(cgl_v3(0, 0.2f, 0), kEye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(forward, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, forward);
  float half_h = tanf(45.0f * CGL_PI / 180.0f * 0.5f);
  float half_w = half_h * ((float)w / (float)h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sx = (2.0f * (x + 0.5f) / w - 1.0f) * half_w;
      float sy = (1.0f - 2.0f * (y + 0.5f) / h) * half_h;
      cgl_vec3 rd = cgl_normalize3(
          cgl_add3(cgl_add3(forward, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
      float best = 1e30f;
      int bi = -1;
      cgl_vec3 bn;
      for (int i = 0; i < 5; ++i) {
        float t;
        cgl_vec3 n;
        if (hit_sphere(kEye, rd, centers[i], 0.9f, &t, &n) && t < best) {
          best = t;
          bi = i;
          bn = n;
        }
      }
      cgl_vec3 col;
      if (bi < 0) {
        col = cgl_lerp3(cgl_v3(0.12f, 0.12f, 0.16f), cgl_v3(0.25f, 0.3f, 0.4f),
                        0.5f * (rd.y + 1.0f));
      } else {
        cgl_vec3 p = cgl_add3(kEye, cgl_scale3(rd, best));
        col = shade(models[bi], p, bn, albedos[bi], 48.0f, 0.65f);
      }
      cgl_image_set(img, x, y, col);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab09: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab09: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
