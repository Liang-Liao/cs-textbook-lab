#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

typedef struct {
  cgl_mat4 local_to_world;
  cgl_vec3 albedo;
} Obj;

static cgl_vec3 shade(cgl_vec3 n, cgl_vec3 albedo) {
  cgl_vec3 l = cgl_normalize3(cgl_v3(-0.35f, -1.0f, -0.25f));
  float ndotl = cgl_clampf(cgl_dot3(n, l), 0.0f, 1.0f);
  return cgl_add3(cgl_scale3(albedo, 0.15f),
                  cgl_scale3(cgl_mul3(albedo, cgl_v3(1.1f, 1.05f, 1.0f)), ndotl));
}

static int hit_unit_sphere(const cgl_vec3 ro, const cgl_vec3 rd, float *t,
                           cgl_vec3 *n) {
  float a = cgl_dot3(rd, rd);
  float b = 2.0f * cgl_dot3(ro, rd);
  float c = cgl_dot3(ro, ro) - 1.0f;
  float disc = b * b - 4.0f * a * c;
  if (disc < 0.0f) {
    return 0;
  }
  float sq = sqrtf(disc);
  float t0 = (-b - sq) / (2.0f * a);
  float t1 = (-b + sq) / (2.0f * a);
  float tt = t0 > 1e-3f ? t0 : t1;
  if (tt < 1e-3f) {
    return 0;
  }
  cgl_vec3 lp = cgl_add3(ro, cgl_scale3(rd, tt));
  *t = tt;
  *n = lp; /* unit sphere normal = local position */
  return 1;
}

static cgl_vec3 render_scene(const Obj *objs, int n, cgl_vec3 ro, cgl_vec3 rd) {
  float best = 1e30f;
  int bi = -1;
  cgl_vec3 bn;
  for (int i = 0; i < n; ++i) {
    cgl_mat4 inv;
    if (!cgl_mat4_invert(&objs[i].local_to_world, &inv)) {
      continue;
    }
    cgl_vec3 lo = cgl_mat4_mul_point(&inv, ro);
    cgl_vec3 ld = cgl_mat4_mul_dir(&inv, rd);
    float t;
    cgl_vec3 ln;
    if (!hit_unit_sphere(lo, ld, &t, &ln)) {
      continue;
    }
    /* approximate world t using local hit transformed */
    cgl_vec3 lp = cgl_add3(lo, cgl_scale3(ld, t));
    cgl_vec3 wp = cgl_mat4_mul_point(&objs[i].local_to_world, lp);
    float tw = cgl_len3(cgl_sub3(wp, ro));
    if (tw < best) {
      best = tw;
      bi = i;
      /* normal: transform by inverse-transpose ≈ M^{-T} n */
      cgl_mat4 invT;
      for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
          invT.m[c * 4 + r] = inv.m[r * 4 + c];
        }
      }
      bn = cgl_normalize3(cgl_mat4_mul_dir(&invT, ln));
    }
  }
  if (bi < 0) {
    float t = 0.5f * (cgl_normalize3(rd).y + 1.0f);
    return cgl_lerp3(cgl_v3(0.85f, 0.85f, 0.9f), cgl_v3(0.35f, 0.55f, 0.85f), t);
  }
  return shade(bn, objs[bi].albedo);
}

static int self_test(void) {
  cgl_mat4 T = cgl_translate(cgl_v3(1, 0, 0));
  cgl_mat4 S = cgl_scale(cgl_v3(2, 2, 2));
  cgl_mat4 R = cgl_rotate_z(CGL_PI * 0.5f);
  cgl_mat4 TS = cgl_mat4_mul(&T, &S);
  cgl_vec3 p = cgl_mat4_mul_point(&TS, cgl_v3(1, 0, 0));
  CGL_EXPECT_NEAR(p.x, 3.0f, 1e-4, "T*S*(1,0,0)");
  cgl_vec3 q = cgl_mat4_mul_point(&R, cgl_v3(1, 0, 0));
  CGL_EXPECT_NEAR(q.x, 0.0f, 1e-4, "Rz90 x");
  CGL_EXPECT_NEAR(q.y, 1.0f, 1e-4, "Rz90 y");
  cgl_mat4 inv, id;
  CGL_EXPECT(cgl_mat4_invert(&TS, &inv), "invert TS");
  id = cgl_mat4_mul(&inv, &TS);
  CGL_EXPECT_NEAR(id.m[0], 1.0f, 1e-4, "I00");
  CGL_EXPECT_NEAR(id.m[5], 1.0f, 1e-4, "I11");
  CGL_EXPECT_NEAR(id.m[10], 1.0f, 1e-4, "I22");
  CGL_EXPECT_NEAR(id.m[15], 1.0f, 1e-4, "I33");
  return cgl_selftest_report("lab05-transformation-matrices");
}

static int write_scene(const char *path, const Obj *objs, int n) {
  int w = 320, h = 180;
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    fprintf(stderr, "lab05: image create failed (%dx%d)\n", w, h);
    return -1;
  }
  cgl_vec3 eye = cgl_v3(0, 1.5f, 6.0f);
  cgl_vec3 forward = cgl_normalize3(cgl_v3(0, -0.15f, -1));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(forward, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, forward);
  float half_h = tanf(40.0f * CGL_PI / 180.0f * 0.5f);
  float half_w = half_h * ((float)w / (float)h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sx = (2.0f * (x + 0.5f) / w - 1.0f) * half_w;
      float sy = (1.0f - 2.0f * (y + 0.5f) / h) * half_h;
      cgl_vec3 dir = cgl_normalize3(
          cgl_add3(cgl_add3(forward, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
      cgl_image_set(img, x, y, render_scene(objs, n, eye, dir));
    }
  }
  if (cgl_image_write_ppm_srgb(img, path) != 0) {
    fprintf(stderr, "lab05: failed to write %s\n", path);
    cgl_image_free(img);
    return -1;
  }
  cgl_image_free(img);
  return 0;
}

int main(int argc, char **argv) {
  const char *prefix = "out/lab05_xform";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out-prefix") && i + 1 < argc) {
      prefix = argv[++i];
    }
  }
  char path[256];
  int fail = 0;
  Obj base[1];
  base[0].albedo = cgl_v3(0.85f, 0.35f, 0.25f);
  base[0].local_to_world = cgl_mat4_identity();

  snprintf(path, sizeof path, "%s_0_identity.ppm", prefix);
  fail |= write_scene(path, base, 1) != 0;

  base[0].local_to_world = cgl_translate(cgl_v3(0.8f, 0.2f, 0));
  snprintf(path, sizeof path, "%s_1_translate.ppm", prefix);
  fail |= write_scene(path, base, 1) != 0;

  cgl_mat4 S = cgl_scale(cgl_v3(1.4f, 0.7f, 1.0f));
  cgl_mat4 R = cgl_rotate_z(0.6f);
  base[0].local_to_world = cgl_mat4_mul(&R, &S);
  snprintf(path, sizeof path, "%s_2_rotate_scale.ppm", prefix);
  fail |= write_scene(path, base, 1) != 0;

  cgl_mat4 T = cgl_translate(cgl_v3(-0.5f, 0.3f, 0.2f));
  cgl_mat4 Ry = cgl_rotate_y(0.8f);
  cgl_mat4 m = cgl_mat4_mul(&T, &Ry);
  base[0].local_to_world = cgl_mat4_mul(&m, &S);
  snprintf(path, sizeof path, "%s_3_composite.ppm", prefix);
  fail |= write_scene(path, base, 1) != 0;

  if (fail) {
    return 1;
  }
  printf("lab05: wrote %s_*.ppm\n", prefix);
  return 0;
}
