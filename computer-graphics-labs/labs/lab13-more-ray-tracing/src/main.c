#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_rng.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define T_MIN 1e-3f
#define MAX_DEPTH 6

typedef struct {
  cgl_vec3 center;
  float radius;
  cgl_vec3 albedo;
  int emissive;
  cgl_vec3 emission;
} Sphere;

typedef struct {
  cgl_vec3 min;
  cgl_vec3 max;
  cgl_vec3 albedo;
  int emissive;
  cgl_vec3 emission;
} Box;

typedef struct {
  cgl_vec3 o, d;
} Ray;

static const Sphere kSpheres[] = {
    {{-1.05f, 0.0f, 0.2f}, 0.95f, {0.85f, 0.18f, 0.15f}, 0, {0, 0, 0}},
    {{1.15f, -0.05f, -0.3f}, 1.0f, {0.2f, 0.4f, 0.9f}, 0, {0, 0, 0}},
    {{0.0f, -1.9f, 0.0f}, 0.5f, {0.9f, 0.85f, 0.3f}, 0, {0, 0, 0}},
};
static const Box kLight = {{-0.9f, 3.2f, -0.9f}, {0.9f, 3.35f, 0.9f},
                           {0, 0, 0}, 1, {18.0f, 16.5f, 14.0f}};
static const cgl_vec3 kBackWall = {0.0f, 0.0f, -1.6f};

static int hit_sphere(const Ray *r, const Sphere *s, float tmax, float *t,
                      cgl_vec3 *n) {
  cgl_vec3 oc = cgl_sub3(r->o, s->center);
  float a = cgl_dot3(r->d, r->d);
  float b = 2.0f * cgl_dot3(oc, r->d);
  float c = cgl_dot3(oc, oc) - s->radius * s->radius;
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
  *n = cgl_normalize3(cgl_sub3(p, s->center));
  return 1;
}

static int hit_plane_y(const Ray *r, float y, float tmax, float *t, cgl_vec3 *n) {
  if (fabsf(r->d.y) < 1e-8f) {
    return 0;
  }
  float tt = (y - r->o.y) / r->d.y;
  if (tt < T_MIN || tt > tmax) {
    return 0;
  }
  *t = tt;
  *n = cgl_v3(0, 1, 0);
  return 1;
}

static int hit_plane_z(const Ray *r, float z, float tmax, float *t, cgl_vec3 *n) {
  if (fabsf(r->d.z) < 1e-8f) {
    return 0;
  }
  float tt = (z - r->o.z) / r->d.z;
  if (tt < T_MIN || tt > tmax) {
    return 0;
  }
  *t = tt;
  *n = cgl_v3(0, 0, 1);
  return 1;
}

static int hit_box(const Ray *r, const Box *b, float tmax, float *t,
                   cgl_vec3 *n) {
  float t0 = T_MIN, t1 = tmax;
  cgl_vec3 n0 = cgl_v3(0, 0, 0);
  const float *od = &r->o.x;
  const float *dd = &r->d.x;
  const float *mn = &b->min.x;
  const float *mx = &b->max.x;
  for (int a = 0; a < 3; ++a) {
    if (fabsf(dd[a]) < 1e-12f) {
      if (od[a] < mn[a] || od[a] > mx[a]) {
        return 0;
      }
      continue;
    }
    float inv = 1.0f / dd[a];
    float ta = (mn[a] - od[a]) * inv;
    float tb = (mx[a] - od[a]) * inv;
    if (ta > tb) {
      float tmp = ta;
      ta = tb;
      tb = tmp;
    }
    cgl_vec3 axisn = cgl_v3(a == 0, a == 1, a == 2);
    if (inv > 0) {
      axisn = cgl_neg3(axisn);
    }
    if (ta > t0) {
      t0 = ta;
      n0 = axisn;
    }
    if (tb < t1) {
      t1 = tb;
    }
    if (t0 > t1) {
      return 0;
    }
  }
  if (t0 < T_MIN || t0 > tmax) {
    return 0;
  }
  *t = t0;
  *n = n0;
  return 1;
}

static cgl_vec3 cosine_hemisphere(cgl_rng *rng, cgl_vec3 n) {
  float u1 = cgl_rng_next01(rng);
  float u2 = cgl_rng_next01(rng);
  float r = sqrtf(u1);
  float phi = 2.0f * CGL_PI * u2;
  float x = r * cosf(phi);
  float y = r * sinf(phi);
  float z = sqrtf(fmaxf(0.0f, 1.0f - u1));
  cgl_vec3 t = fabsf(n.x) > 0.5f ? cgl_v3(0, 1, 0) : cgl_v3(1, 0, 0);
  cgl_vec3 a = cgl_normalize3(cgl_cross3(t, n));
  cgl_vec3 b = cgl_cross3(n, a);
  return cgl_normalize3(
      cgl_add3(cgl_scale3(a, x), cgl_add3(cgl_scale3(b, y), cgl_scale3(n, z))));
}

static int scene_hit(const Ray *r, float *t, cgl_vec3 *n, cgl_vec3 *albedo,
                     cgl_vec3 *emission, int *specular) {
  float best = 1e30f;
  int hit = 0;
  *specular = 0;
  *emission = cgl_v3(0, 0, 0);
  float tt;
  cgl_vec3 nn;
  if (hit_box(r, &kLight, best, &tt, &nn) && tt < best) {
    best = tt;
    *n = nn;
    *albedo = cgl_v3(0, 0, 0);
    *emission = kLight.emission;
    hit = 1;
  }
  if (hit_plane_y(r, -1.2f, best, &tt, &nn) && tt < best) {
    best = tt;
    *n = nn;
    cgl_vec3 p = cgl_add3(r->o, cgl_scale3(r->d, tt));
    int cx = (int)floorf(p.x * 0.7f);
    int cz = (int)floorf(p.z * 0.7f);
    float s = ((cx + cz) & 1) ? 0.55f : 0.75f;
    *albedo = cgl_v3(s, s, s * 0.98f);
    hit = 1;
  }
  if (hit_plane_z(r, kBackWall.z, best, &tt, &nn) && tt < best) {
    best = tt;
    *n = nn;
    *albedo = cgl_v3(0.72f, 0.72f, 0.75f);
    hit = 1;
  }
  for (size_t i = 0; i < sizeof kSpheres / sizeof kSpheres[0]; ++i) {
    if (hit_sphere(r, &kSpheres[i], best, &tt, &nn) && tt < best) {
      best = tt;
      *n = nn;
      *albedo = kSpheres[i].albedo;
      hit = 1;
    }
  }
  if (hit) {
    *t = best;
  }
  return hit;
}

static cgl_vec3 sample_light_point(cgl_rng *rng) {
  return cgl_v3(cgl_lerpf(kLight.min.x, kLight.max.x, cgl_rng_next01(rng)),
                kLight.min.y,
                cgl_lerpf(kLight.min.z, kLight.max.z, cgl_rng_next01(rng)));
}

static int visibility(cgl_vec3 from, cgl_vec3 to) {
  cgl_vec3 d = cgl_sub3(to, from);
  float dist = cgl_len3(d);
  Ray r = {cgl_add3(from, cgl_scale3(cgl_normalize3(d), 1e-3f)),
           cgl_normalize3(d)};
  float t;
  cgl_vec3 n, albedo, emission;
  int spec;
  if (!scene_hit(&r, &t, &n, &albedo, &emission, &spec)) {
    return 1;
  }
  return t > dist - 2e-3f;
}

static cgl_vec3 trace_path(const Ray *primary, cgl_rng *rng) {
  cgl_vec3 L = cgl_v3(0, 0, 0);
  cgl_vec3 beta = cgl_v3(1, 1, 1);
  Ray ray = *primary;
  for (int depth = 0; depth < MAX_DEPTH; ++depth) {
    float t;
    cgl_vec3 n, albedo, emission;
    int spec;
    if (!scene_hit(&ray, &t, &n, &albedo, &emission, &spec)) {
      L = cgl_add3(L, cgl_mul3(beta, cgl_v3(0.02f, 0.03f, 0.05f)));
      break;
    }
    cgl_vec3 p = cgl_add3(ray.o, cgl_scale3(ray.d, t));
    if (cgl_dot3(n, ray.d) > 0) {
      n = cgl_neg3(n);
    }
    if (emission.x > 0 || emission.y > 0 || emission.z > 0) {
      L = cgl_add3(L, cgl_mul3(beta, emission));
      break;
    }
    /* Next event estimation to area light */
    cgl_vec3 lp = sample_light_point(rng);
    cgl_vec3 to_l = cgl_sub3(lp, p);
    float dist2 = cgl_len2_3(to_l);
    cgl_vec3 wi = cgl_normalize3(to_l);
    float ndotl = cgl_dot3(n, wi);
    if (ndotl > 0 && visibility(p, lp)) {
      /* Bottom face of the area light faces -y; cos between n_l=-y and -wi is wi.y. */
      float cos_light = cgl_clampf(wi.y, 1e-4f, 1.0f);
      float area = (kLight.max.x - kLight.min.x) * (kLight.max.z - kLight.min.z);
      float pdf = dist2 / (cgl_clampf(cos_light, 1e-4f, 1.0f) * area);
      /* cosine-weighted pdf for lambert would be ndotl/pi; use NEE with light pdf */
      cgl_vec3 brdf = cgl_scale3(albedo, 1.0f / CGL_PI);
      float w = ndotl / fmaxf(pdf, 1e-8f);
      L = cgl_add3(L, cgl_mul3(cgl_mul3(beta, brdf), cgl_scale3(kLight.emission, w)));
    }
    /* cosine bounce */
    cgl_vec3 newd = cosine_hemisphere(rng, n);
    float ndotd = cgl_clampf(cgl_dot3(n, newd), 0.0f, 1.0f);
    /* pdf = ndotd/pi, brdf = albedo/pi => beta *= albedo */
    beta = cgl_mul3(beta, albedo);
    ray.o = cgl_add3(p, cgl_scale3(n, 1e-3f));
    ray.d = newd;
    if (depth >= 2) {
      float q = cgl_clampf(fmaxf(beta.x, fmaxf(beta.y, beta.z)), 0.05f, 0.95f);
      if (cgl_rng_next01(rng) > q) {
        break;
      }
      beta = cgl_scale3(beta, 1.0f / q);
    }
    (void)ndotd;
  }
  return L;
}

static int self_test(void) {
  cgl_rng rng;
  cgl_rng_seed(&rng, 42);
  float sum = 0.0f;
  for (int i = 0; i < 1000; ++i) {
    float u = cgl_rng_next01(&rng);
    CGL_EXPECT(u >= 0.0f && u < 1.0f, "rng range");
    sum += u;
  }
  float mean = sum / 1000.0f;
  CGL_EXPECT(mean > 0.4f && mean < 0.6f, "rng mean ~0.5");

  Ray r = {cgl_v3(0, 0, 5), cgl_v3(0, 0, -1)};
  float t;
  cgl_vec3 n, albedo, emission;
  int spec;
  CGL_EXPECT(scene_hit(&r, &t, &n, &albedo, &emission, &spec), "scene hit");
  CGL_EXPECT(t > 0, "positive t");
  CGL_EXPECT(visibility(cgl_v3(0, 0, 0), cgl_v3(0, 2.5f, 0)), "vis to light");
  CGL_EXPECT(!visibility(cgl_v3(0, 0, 0), cgl_v3(5, 0, 0)), "blocked by blue sphere");
  /* Parallel-axis slab: ray along +z, origin inside light x/y range. */
  Ray par = {cgl_v3(0.0f, 3.275f, -2.0f), cgl_v3(0.0f, 0.0f, 1.0f)};
  CGL_EXPECT(hit_box(&par, &kLight, 1e30f, &t, &n), "parallel axis hits light");
  CGL_EXPECT_NEAR(t, 1.1f, 1e-3, "parallel t to light zmin");
  return cgl_selftest_report("lab13-more-ray-tracing");
}

int main(int argc, char **argv) {
  int w = 320, h = 180, spp = 32;
  uint64_t seed = 7;
  const char *out = "out/lab13_pathtrace.ppm";
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
    }
  }
  if (spp < 1) {
    spp = 1;
  }
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  cgl_vec3 eye = cgl_v3(0, 1.4f, 5.5f);
  cgl_vec3 forward = cgl_normalize3(cgl_sub3(cgl_v3(0, 0.2f, 0), eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(forward, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, forward);
  float half_h = tanf(40.0f * CGL_PI / 180.0f * 0.5f);
  float half_w = half_h * ((float)w / (float)h);
  cgl_rng rng;
  cgl_rng_seed(&rng, seed);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      cgl_vec3 acc = cgl_v3(0, 0, 0);
      for (int s = 0; s < spp; ++s) {
        float px = (x + cgl_rng_next01(&rng)) / (float)w;
        float py = (y + cgl_rng_next01(&rng)) / (float)h;
        float sx = (2.0f * px - 1.0f) * half_w;
        float sy = (1.0f - 2.0f * py) * half_h;
        cgl_vec3 dir = cgl_normalize3(
            cgl_add3(cgl_add3(forward, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
        Ray ray = {eye, dir};
        acc = cgl_add3(acc, trace_path(&ray, &rng));
      }
      cgl_image_set(img, x, y, cgl_scale3(acc, 1.0f / (float)spp));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab13: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab13: wrote %s (%dx%d spp=%d seed=%llu)\n", out, w, h, spp,
         (unsigned long long)seed);
  cgl_image_free(img);
  return 0;
}
