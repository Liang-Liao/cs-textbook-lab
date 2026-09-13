#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_rng.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define T_MIN 1e-3f

typedef struct {
  cgl_vec3 c;
  float r;
  cgl_vec3 albedo;
  float reflect;
  float refract;
  float ior;
} Sphere;

typedef struct {
  cgl_vec3 o, d;
} Ray;

static const Sphere kScene[] = {
    {{-1.2f, 0, 0}, 1.0f, {0.85f, 0.2f, 0.15f}, 0.05f, 0, 1},
    {{1.15f, 0.05f, -0.2f}, 1.0f, {0.95f, 0.95f, 0.98f}, 0.05f, 1.0f, 1.5f},
    {{0.1f, -1.55f, 0.4f}, 0.45f, {0.9f, 0.8f, 0.3f}, 0.0f, 0, 1},
};
static const cgl_vec3 kLight = {1.2f, 3.5f, 1.5f};
static const float kLightSize = 0.8f;

static int hit_sphere(const Ray *r, const Sphere *s, float tmax, float *t,
                      cgl_vec3 *n) {
  cgl_vec3 oc = cgl_sub3(r->o, s->c);
  float a = cgl_dot3(r->d, r->d);
  float b = 2 * cgl_dot3(oc, r->d);
  float c = cgl_dot3(oc, oc) - s->r * s->r;
  float disc = b * b - 4 * a * c;
  if (disc < 0) {
    return 0;
  }
  float sq = sqrtf(disc);
  float t0 = (-b - sq) / (2 * a);
  float t1 = (-b + sq) / (2 * a);
  float tt = t0 > T_MIN ? t0 : t1;
  if (tt < T_MIN || tt > tmax) {
    return 0;
  }
  cgl_vec3 p = cgl_add3(r->o, cgl_scale3(r->d, tt));
  *t = tt;
  *n = cgl_normalize3(cgl_sub3(p, s->c));
  return 1;
}

static int scene_hit(const Ray *r, float *t, int *id, cgl_vec3 *n) {
  float best = 1e30f;
  int bi = -1;
  for (int i = 0; i < 3; ++i) {
    float tt;
    cgl_vec3 nn;
    if (hit_sphere(r, &kScene[i], best, &tt, &nn) && tt < best) {
      best = tt;
      bi = i;
      *n = nn;
    }
  }
  float tp;
  if (fabsf(r->d.y) > 1e-8f) {
    tp = (-2.0f - r->o.y) / r->d.y;
    if (tp > T_MIN && tp < best) {
      best = tp;
      bi = 100;
      *n = cgl_v3(0, 1, 0);
    }
  }
  if (bi < 0) {
    return 0;
  }
  *t = best;
  *id = bi;
  return 1;
}

static int occluded(cgl_vec3 p, cgl_vec3 to) {
  cgl_vec3 d = cgl_sub3(to, p);
  float dist = cgl_len3(d);
  Ray r = {cgl_add3(p, cgl_scale3(cgl_normalize3(d), 1e-3f)),
           cgl_normalize3(d)};
  float t;
  int id;
  cgl_vec3 n;
  if (!scene_hit(&r, &t, &id, &n)) {
    return 0;
  }
  return t < dist - 1e-2f;
}

static float fresnel(float cosi, float etai, float etat) {
  float sint = etai / etat * sqrtf(fmaxf(0.0f, 1.0f - cosi * cosi));
  if (sint >= 1.0f) {
    return 1.0f;
  }
  float cost = sqrtf(fmaxf(0.0f, 1.0f - sint * sint));
  cosi = fabsf(cosi);
  float rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
  float rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
  return 0.5f * (rs * rs + rp * rp);
}

static cgl_vec3 shade_lambert(cgl_vec3 p, cgl_vec3 n, cgl_vec3 albedo,
                              cgl_rng *rng) {
  cgl_vec3 lp = cgl_add3(kLight, cgl_v3((cgl_rng_next01(rng) - 0.5f) * kLightSize,
                                        0,
                                        (cgl_rng_next01(rng) - 0.5f) * kLightSize));
  cgl_vec3 L = cgl_sub3(lp, p);
  float dist2 = cgl_len2_3(L);
  cgl_vec3 wi = cgl_normalize3(L);
  float ndotl = cgl_clampf(cgl_dot3(n, wi), 0, 1);
  int vis = ndotl > 0 && !occluded(p, lp);
  float att = 1.0f / (0.5f + 0.08f * dist2);
  return cgl_add3(cgl_scale3(albedo, 0.08f),
                  cgl_scale3(cgl_mul3(albedo, cgl_v3(1.2f, 1.15f, 1.05f)),
                             ndotl * att * (vis ? 1.0f : 0.05f)));
}

static cgl_vec3 trace(const Ray *primary, cgl_rng *rng, int depth) {
  if (depth <= 0) {
    return cgl_v3(0.05f, 0.06f, 0.08f);
  }
  float t;
  int id;
  cgl_vec3 n;
  if (!scene_hit(primary, &t, &id, &n)) {
    float y = cgl_normalize3(primary->d).y;
    return cgl_lerp3(cgl_v3(0.7f, 0.75f, 0.85f), cgl_v3(0.25f, 0.45f, 0.8f),
                     0.5f * (y + 1));
  }
  cgl_vec3 p = cgl_add3(primary->o, cgl_scale3(primary->d, t));
  if (id == 100) {
    int cx = (int)floorf(p.x * 0.6f);
    int cz = (int)floorf(p.z * 0.6f);
    float s = ((cx + cz) & 1) ? 0.55f : 0.8f;
    return shade_lambert(p, n, cgl_v3(s, s, s * 0.98f), rng);
  }
  const Sphere *s = &kScene[id];
  if (s->refract > 0) {
    cgl_vec3 outward = n;
    float etai = 1.0f, etat = s->ior;
    if (cgl_dot3(primary->d, n) > 0) {
      outward = cgl_neg3(n);
      etai = s->ior;
      etat = 1.0f;
    }
    float cosi = -cgl_dot3(primary->d, outward);
    float f = fresnel(cosi, etai, etat);
    cgl_vec3 refl_dir = cgl_reflect3(primary->d, outward);
    Ray rr = {cgl_add3(p, cgl_scale3(outward, 1e-3f)), cgl_normalize3(refl_dir)};
    cgl_vec3 rc = trace(&rr, rng, depth - 1);
    cgl_vec3 trans;
    cgl_vec3 refr_col = cgl_v3(0, 0, 0);
    if (cgl_refract3(cgl_normalize3(primary->d), outward, etai / etat, &trans)) {
      Ray rt = {cgl_add3(p, cgl_scale3(outward, -1e-3f)), cgl_normalize3(trans)};
      refr_col = trace(&rt, rng, depth - 1);
    }
    return cgl_lerp3(refr_col, rc, f);
  }
  cgl_vec3 col = shade_lambert(p, n, s->albedo, rng);
  if (s->reflect > 0) {
    Ray rr = {cgl_add3(p, cgl_scale3(n, 1e-3f)),
              cgl_normalize3(cgl_reflect3(primary->d, n))};
    cgl_vec3 rc = trace(&rr, rng, depth - 1);
    col = cgl_lerp3(col, rc, s->reflect);
  }
  return col;
}

static int self_test(void) {
  cgl_rng rng;
  cgl_rng_seed(&rng, 1);
  cgl_vec3 t;
  CGL_EXPECT(cgl_refract3(cgl_v3(0, 0, 1), cgl_v3(0, 0, -1), 1.0f, &t),
             "refract air-air");
  CGL_EXPECT_NEAR(fresnel(1.0f, 1.0f, 1.5f), 0.0f, 0.05f, "normal incidence fresnel small");
  CGL_EXPECT(fresnel(0.0f, 1.0f, 1.5f) > 0.5f, "grazing fresnel large");
  Ray r = {cgl_v3(-1.2f, 0, 5), cgl_v3(0, 0, -1)};
  float tt;
  int id;
  cgl_vec3 n;
  CGL_EXPECT(scene_hit(&r, &tt, &id, &n), "hit scene");
  return cgl_selftest_report("lab10-rays-and-more");
}

int main(int argc, char **argv) {
  int w = 400, h = 225, spp = 16;
  uint64_t seed = 11;
  const char *out = "out/lab10_rays.ppm";
  float lens_r = 0.12f;
  float focus = 5.5f;
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--spp") && i + 1 < argc) {
      spp = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
      seed = (uint64_t)strtoull(argv[++i], NULL, 10);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--lens") && i + 1 < argc) {
      lens_r = (float)atof(argv[++i]);
    }
  }
  if (spp < 1) spp = 1;
  cgl_image *img = cgl_image_create(w, h);
  if (!img) return 1;
  cgl_vec3 eye = cgl_v3(0, 1.0f, 5.5f);
  cgl_vec3 target = cgl_v3(0, 0, 0);
  cgl_vec3 fwd = cgl_normalize3(cgl_sub3(target, eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(fwd, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, fwd);
  float half_h = tanf(40 * CGL_PI / 180 * 0.5f);
  float half_w = half_h * ((float)w / h);
  cgl_rng rng;
  cgl_rng_seed(&rng, seed);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      cgl_vec3 acc = cgl_v3(0, 0, 0);
      for (int s = 0; s < spp; ++s) {
        float px = (x + cgl_rng_next01(&rng)) / (float)w;
        float py = (y + cgl_rng_next01(&rng)) / (float)h;
        float sx = (2 * px - 1) * half_w;
        float sy = (1 - 2 * py) * half_h;
        cgl_vec3 dir = cgl_normalize3(
            cgl_add3(cgl_add3(fwd, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
        /* thin lens */
        float u = cgl_rng_next01(&rng);
        float v = cgl_rng_next01(&rng);
        float r = lens_r * sqrtf(u);
        float phi = 2 * CGL_PI * v;
        cgl_vec3 lens_off = cgl_add3(cgl_scale3(right, r * cosf(phi)),
                                     cgl_scale3(up, r * sinf(phi)));
        cgl_vec3 focal_pt = cgl_add3(eye, cgl_scale3(dir, focus));
        Ray ray = {cgl_add3(eye, lens_off),
                   cgl_normalize3(cgl_sub3(focal_pt, cgl_add3(eye, lens_off)))};
        acc = cgl_add3(acc, trace(&ray, &rng, 5));
      }
      cgl_image_set(img, x, y, cgl_scale3(acc, 1.0f / (float)spp));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab10: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab10: wrote %s spp=%d\n", out, spp);
  cgl_image_free(img);
  return 0;
}
