#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static cgl_vec3 ray_color(cgl_vec3 ro, cgl_vec3 rd) {
  /* 5 unit spheres along x */
  for (int i = 0; i < 5; ++i) {
    cgl_vec3 c = cgl_v3((float)i - 2.0f, 0, 0);
    cgl_vec3 oc = cgl_sub3(ro, c);
    float a = cgl_dot3(rd, rd);
    float b = 2.0f * cgl_dot3(oc, rd);
    float cc = cgl_dot3(oc, oc) - 0.55f * 0.55f;
    float disc = b * b - 4 * a * cc;
    if (disc >= 0) {
      float t = (-b - sqrtf(disc)) / (2 * a);
      if (t > 1e-3f) {
        cgl_vec3 p = cgl_add3(ro, cgl_scale3(rd, t));
        cgl_vec3 n = cgl_normalize3(cgl_sub3(p, c));
        float ndotl = cgl_clampf(cgl_dot3(n, cgl_normalize3(cgl_v3(-0.4f, -1, -0.3f))), 0, 1);
        cgl_vec3 albedo = cgl_v3(0.3f + 0.15f * i, 0.55f, 0.85f - 0.12f * i);
        return cgl_add3(cgl_scale3(albedo, 0.2f), cgl_scale3(albedo, ndotl));
      }
    }
  }
  /* ground */
  if (rd.y < -1e-6f) {
    float t = (0.7f - ro.y) / rd.y;
    if (t > 0) {
      cgl_vec3 p = cgl_add3(ro, cgl_scale3(rd, t));
      int cx = (int)floorf(p.x);
      int cz = (int)floorf(p.z);
      float s = ((cx + cz) & 1) ? 0.45f : 0.75f;
      return cgl_v3(s, s, s + 0.02f);
    }
  }
  float t = 0.5f * (cgl_normalize3(rd).y + 1.0f);
  return cgl_lerp3(cgl_v3(0.85f, 0.85f, 0.9f), cgl_v3(0.4f, 0.6f, 0.9f), t);
}

static int self_test(void) {
  cgl_vec3 eye = cgl_v3(0, 0, 5);
  cgl_mat4 V = cgl_look_at(eye, cgl_v3(0, 0, 0), cgl_v3(0, 1, 0));
  cgl_vec3 o = cgl_mat4_mul_point(&V, eye);
  CGL_EXPECT_NEAR(o.x, 0, 1e-4, "eye->origin x");
  CGL_EXPECT_NEAR(o.y, 0, 1e-4, "eye->origin y");
  CGL_EXPECT_NEAR(o.z, 0, 1e-4, "eye->origin z");
  cgl_vec3 front = cgl_mat4_mul_point(&V, cgl_v3(0, 0, 0));
  CGL_EXPECT(front.z < 0.0f, "target in front has negative z (RH)");
  cgl_mat4 P = cgl_perspective(CGL_PI / 3.0f, 1.0f, 0.1f, 100.0f);
  cgl_vec4 clip = cgl_mat4_mulv(&P, cgl_v4(0, 0, -1, 1));
  CGL_EXPECT(fabsf(clip.w) > 1e-6f, "perspective w nonzero");
  return cgl_selftest_report("lab06-viewing");
}

typedef enum { VIEW_PERSPECTIVE, VIEW_ORTHO } ViewKind;

static int write_view(const char *path, cgl_vec3 eye, cgl_vec3 target,
                      float fovy_deg, ViewKind kind) {
  int w = 400, h = 225;
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    fprintf(stderr, "lab06: image create failed (%dx%d)\n", w, h);
    return -1;
  }
  cgl_vec3 forward = cgl_normalize3(cgl_sub3(target, eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(forward, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, forward);
  float half_h = tanf(fovy_deg * CGL_PI / 180.0f * 0.5f);
  float half_w = half_h * ((float)w / (float)h);
  float ortho_h = 1.6f;
  float ortho_w = ortho_h * ((float)w / (float)h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sx, sy;
      cgl_vec3 dir;
      if (kind == VIEW_PERSPECTIVE) {
        sx = (2.0f * (x + 0.5f) / w - 1.0f) * half_w;
        sy = (1.0f - 2.0f * (y + 0.5f) / h) * half_h;
        dir = cgl_normalize3(
            cgl_add3(cgl_add3(forward, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
      } else {
        sx = (2.0f * (x + 0.5f) / w - 1.0f) * ortho_w;
        sy = (1.0f - 2.0f * (y + 0.5f) / h) * ortho_h;
        dir = forward;
        cgl_vec3 o = cgl_add3(cgl_add3(eye, cgl_scale3(right, sx)),
                              cgl_scale3(up, sy));
        cgl_image_set(img, x, y, ray_color(o, dir));
        continue;
      }
      cgl_image_set(img, x, y, ray_color(eye, dir));
    }
  }
  int ok = cgl_image_write_ppm_srgb(img, path) == 0;
  if (!ok) {
    fprintf(stderr, "lab06: failed to write %s\n", path);
  }
  cgl_image_free(img);
  return ok ? 0 : -1;
}

int main(int argc, char **argv) {
  const char *prefix = "out/lab06_view";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out-prefix") && i + 1 < argc) {
      prefix = argv[++i];
    }
  }
  char path[256];
  int fail = 0;
  cgl_vec3 eye = cgl_v3(0, 1.8f, 7.0f);
  cgl_vec3 target = cgl_v3(0, 0, 0);
  snprintf(path, sizeof path, "%s_0_persp.ppm", prefix);
  fail |= write_view(path, eye, target, 50.0f, VIEW_PERSPECTIVE) != 0;
  snprintf(path, sizeof path, "%s_1_wide.ppm", prefix);
  fail |= write_view(path, eye, target, 90.0f, VIEW_PERSPECTIVE) != 0;
  snprintf(path, sizeof path, "%s_2_ortho.ppm", prefix);
  fail |= write_view(path, eye, target, 50.0f, VIEW_ORTHO) != 0;
  if (fail) {
    return 1;
  }
  printf("lab06: wrote %s_*.ppm\n", prefix);
  return 0;
}
