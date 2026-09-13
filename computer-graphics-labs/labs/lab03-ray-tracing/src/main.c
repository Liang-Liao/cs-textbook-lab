#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define T_MIN 1e-3f
#define T_MAX 1e30f

typedef struct {
  cgl_vec3 center;
  float radius;
  cgl_vec3 albedo;
  float reflectivity;
} Sphere;

typedef struct {
  cgl_vec3 origin;
  cgl_vec3 dir;
} Ray;

static const cgl_vec3 kLightDir = {-0.4f, -1.0f, -0.3f}; /* toward scene */
static const cgl_vec3 kLightColor = {1.2f, 1.15f, 1.05f};
static const cgl_vec3 kSkyTop = {0.45f, 0.65f, 0.95f};
static const cgl_vec3 kSkyBot = {0.9f, 0.9f, 0.95f};

static int hit_sphere(const Ray *r, const Sphere *s, float tmin, float tmax,
                      float *t_out, cgl_vec3 *n_out) {
  cgl_vec3 oc = cgl_sub3(r->origin, s->center);
  float a = cgl_dot3(r->dir, r->dir);
  float b = 2.0f * cgl_dot3(oc, r->dir);
  float c = cgl_dot3(oc, oc) - s->radius * s->radius;
  float disc = b * b - 4.0f * a * c;
  if (disc < 0.0f) {
    return 0;
  }
  float sq = sqrtf(disc);
  float t = (-b - sq) / (2.0f * a);
  if (t < tmin || t > tmax) {
    t = (-b + sq) / (2.0f * a);
    if (t < tmin || t > tmax) {
      return 0;
    }
  }
  cgl_vec3 p = cgl_add3(r->origin, cgl_scale3(r->dir, t));
  *t_out = t;
  *n_out = cgl_normalize3(cgl_sub3(p, s->center));
  return 1;
}

static int hit_plane(const Ray *r, float y, float *t_out, cgl_vec3 *n_out) {
  if (fabsf(r->dir.y) < 1e-8f) {
    return 0;
  }
  float t = (y - r->origin.y) / r->dir.y;
  if (t < T_MIN) {
    return 0;
  }
  *t_out = t;
  *n_out = cgl_v3(0, 1, 0);
  return 1;
}

static int occluded(const Ray *r, const Sphere *spheres, int ns, float tmax) {
  for (int i = 0; i < ns; ++i) {
    float t;
    cgl_vec3 n;
    if (hit_sphere(r, &spheres[i], T_MIN, tmax, &t, &n)) {
      return 1;
    }
  }
  return 0;
}

static cgl_vec3 shade_diffuse(cgl_vec3 p, cgl_vec3 n, cgl_vec3 albedo,
                              const Sphere *spheres, int ns) {
  cgl_vec3 l = cgl_normalize3(cgl_scale3(kLightDir, -1.0f));
  float ndotl = cgl_clampf(cgl_dot3(n, l), 0.0f, 1.0f);
  Ray shadow = {p, l};
  int hit = occluded(&shadow, spheres, ns, T_MAX);
  float vis = hit ? 0.15f : 1.0f;
  cgl_vec3 ambient = cgl_scale3(albedo, 0.12f);
  cgl_vec3 diff = cgl_scale3(cgl_mul3(albedo, kLightColor), ndotl * vis);
  return cgl_add3(ambient, diff);
}

static cgl_vec3 background(cgl_vec3 d) {
  float t = 0.5f * (d.y + 1.0f);
  return cgl_lerp3(kSkyBot, kSkyTop, t);
}

static cgl_vec3 trace(const Ray *ray, const Sphere *spheres, int ns, int depth) {
  if (depth <= 0) {
    return background(ray->dir);
  }
  float best_t = T_MAX;
  cgl_vec3 best_n;
  int best_i = -1;
  int on_plane = 0;
  for (int i = 0; i < ns; ++i) {
    float t;
    cgl_vec3 n;
    if (hit_sphere(ray, &spheres[i], T_MIN, best_t, &t, &n)) {
      best_t = t;
      best_n = n;
      best_i = i;
      on_plane = 0;
    }
  }
  float tp;
  cgl_vec3 np;
  if (hit_plane(ray, -1.0f, &tp, &np) && tp < best_t) {
    best_t = tp;
    best_n = np;
    best_i = -1;
    on_plane = 1;
  }
  if (best_i < 0 && !on_plane) {
    return background(ray->dir);
  }
  cgl_vec3 p = cgl_add3(ray->origin, cgl_scale3(ray->dir, best_t));
  cgl_vec3 albedo = on_plane ? cgl_v3(0.75f, 0.75f, 0.78f) : spheres[best_i].albedo;
  float refl = on_plane ? 0.08f : spheres[best_i].reflectivity;
  if (on_plane) {
    int cx = (int)floorf(p.x * 0.5f);
    int cz = (int)floorf(p.z * 0.5f);
    if ((cx + cz) & 1) {
      albedo = cgl_scale3(albedo, 0.55f);
    }
  }
  cgl_vec3 col = shade_diffuse(p, best_n, albedo, spheres, ns);
  if (refl > 0.0f) {
    cgl_vec3 rdir = cgl_reflect3(ray->dir, best_n);
    Ray refl_ray = {cgl_add3(p, cgl_scale3(best_n, 1e-3f)), cgl_normalize3(rdir)};
    cgl_vec3 rc = trace(&refl_ray, spheres, ns, depth - 1);
    col = cgl_lerp3(col, rc, refl);
  }
  return col;
}

static int self_test(void) {
  Sphere s = {cgl_v3(0, 0, 0), 1.0f, cgl_v3(1, 0, 0), 0};
  Ray r = {cgl_v3(0, 0, -3), cgl_v3(0, 0, 1)};
  float t;
  cgl_vec3 n;
  CGL_EXPECT(hit_sphere(&r, &s, T_MIN, T_MAX, &t, &n), "hit front");
  CGL_EXPECT_NEAR(t, 2.0f, 1e-4, "t == 2");
  CGL_EXPECT_NEAR(n.z, -1.0f, 1e-4, "normal -z");
  Ray miss = {cgl_v3(0, 5, -3), cgl_v3(0, 0, 1)};
  CGL_EXPECT(!hit_sphere(&miss, &s, T_MIN, T_MAX, &t, &n), "miss");
  cgl_vec3 in = cgl_normalize3(cgl_v3(1, -1, 0));
  cgl_vec3 v = cgl_reflect3(in, cgl_v3(0, 1, 0));
  CGL_EXPECT_NEAR(v.x, in.x, 1e-4, "reflect keeps tangent x");
  CGL_EXPECT_NEAR(v.y, -in.y, 1e-4, "reflect flips normal component");
  return cgl_selftest_report("lab03-ray-tracing");
}

int main(int argc, char **argv) {
  int w = 400, h = 225, depth = 4;
  const char *out = "out/lab03_raytrace.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--depth") && i + 1 < argc) {
      depth = atoi(argv[++i]);
    }
  }
  Sphere spheres[2] = {
      {cgl_v3(-1.1f, 0.0f, 0.0f), 1.0f, cgl_v3(0.85f, 0.15f, 0.12f), 0.0f},
      {cgl_v3(1.2f, 0.1f, -0.4f), 1.1f, cgl_v3(0.15f, 0.35f, 0.85f), 0.45f},
  };
  cgl_vec3 eye = cgl_v3(0, 1.2f, 5.0f);
  cgl_vec3 target = cgl_v3(0, 0.1f, 0);
  cgl_vec3 forward = cgl_normalize3(cgl_sub3(target, eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(forward, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, forward);
  float fov = 50.0f * CGL_PI / 180.0f;
  float aspect = (float)w / (float)h;
  float half_h = tanf(fov * 0.5f);
  float half_w = half_h * aspect;

  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sx = (2.0f * (x + 0.5f) / (float)w - 1.0f) * half_w;
      float sy = (1.0f - 2.0f * (y + 0.5f) / (float)h) * half_h;
      cgl_vec3 dir = cgl_normalize3(
          cgl_add3(cgl_add3(forward, cgl_scale3(right, sx)), cgl_scale3(up, sy)));
      Ray ray = {eye, dir};
      cgl_image_set(img, x, y, trace(&ray, spheres, 2, depth));
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab03: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab03: wrote %s (%dx%d)\n", out, w, h);
  cgl_image_free(img);
  return 0;
}
