#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cgl_image.h"
#include "cgl_rng.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define MAX_SPHERES 512
#define GRID_N 12

typedef struct {
  cgl_vec3 c;
  float r;
  cgl_vec3 color;
} Sph;

typedef struct {
  cgl_vec3 mn, mx;
} AABB;

typedef struct {
  cgl_vec3 o, d;
} Ray;

typedef struct {
  Sph s[MAX_SPHERES];
  int n;
  AABB bounds;
  int cell_head[GRID_N * GRID_N * GRID_N];
  int cell_next[MAX_SPHERES];
} Scene;

static AABB aabb_sphere(const Sph *s) {
  AABB b;
  b.mn = cgl_sub3(s->c, cgl_v3s(s->r));
  b.mx = cgl_add3(s->c, cgl_v3s(s->r));
  return b;
}

static void scene_bounds(Scene *sc) {
  sc->bounds = aabb_sphere(&sc->s[0]);
  for (int i = 1; i < sc->n; ++i) {
    AABB b = aabb_sphere(&sc->s[i]);
    sc->bounds.mn = cgl_min3(sc->bounds.mn, b.mn);
    sc->bounds.mx = cgl_max3(sc->bounds.mx, b.mx);
  }
  sc->bounds.mn = cgl_sub3(sc->bounds.mn, cgl_v3s(0.01f));
  sc->bounds.mx = cgl_add3(sc->bounds.mx, cgl_v3s(0.01f));
}

static int cell_index(int ix, int iy, int iz) {
  return (iz * GRID_N + iy) * GRID_N + ix;
}

static void build_grid(Scene *sc) {
  for (int i = 0; i < GRID_N * GRID_N * GRID_N; ++i) {
    sc->cell_head[i] = -1;
  }
  cgl_vec3 ext = cgl_sub3(sc->bounds.mx, sc->bounds.mn);
  for (int i = 0; i < sc->n; ++i) {
    AABB b = aabb_sphere(&sc->s[i]);
    int x0 = (int)floorf((b.mn.x - sc->bounds.mn.x) / ext.x * GRID_N);
    int x1 = (int)floorf((b.mx.x - sc->bounds.mn.x) / ext.x * GRID_N);
    int y0 = (int)floorf((b.mn.y - sc->bounds.mn.y) / ext.y * GRID_N);
    int y1 = (int)floorf((b.mx.y - sc->bounds.mn.y) / ext.y * GRID_N);
    int z0 = (int)floorf((b.mn.z - sc->bounds.mn.z) / ext.z * GRID_N);
    int z1 = (int)floorf((b.mx.z - sc->bounds.mn.z) / ext.z * GRID_N);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= GRID_N) x1 = GRID_N - 1;
    if (y1 >= GRID_N) y1 = GRID_N - 1;
    if (z1 >= GRID_N) z1 = GRID_N - 1;
    for (int z = z0; z <= z1; ++z) {
      for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
          int id = cell_index(x, y, z);
          sc->cell_next[i] = sc->cell_head[id];
          sc->cell_head[id] = i;
        }
      }
    }
  }
}

static int hit_sphere(const Ray *r, const Sph *s, float *t_out) {
  cgl_vec3 oc = cgl_sub3(r->o, s->c);
  float a = cgl_dot3(r->d, r->d);
  float b = 2 * cgl_dot3(oc, r->d);
  float c = cgl_dot3(oc, oc) - s->r * s->r;
  float disc = b * b - 4 * a * c;
  if (disc < 0) {
    return 0;
  }
  float sq = sqrtf(disc);
  float t = (-b - sq) / (2 * a);
  if (t < 1e-3f) {
    t = (-b + sq) / (2 * a);
  }
  if (t < 1e-3f) {
    return 0;
  }
  *t_out = t;
  return 1;
}

static int hit_aabb(const Ray *r, const AABB *b, float *tmin_out, float *tmax_out) {
  float t0 = -1e30f, t1 = 1e30f;
  const float *od = &r->o.x, *dd = &r->d.x;
  const float *mn = &b->mn.x, *mx = &b->mx.x;
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
    if (ta > t0) t0 = ta;
    if (tb < t1) t1 = tb;
    if (t0 > t1) return 0;
  }
  *tmin_out = t0;
  *tmax_out = t1;
  return 1;
}

static int trace_brute(const Scene *sc, const Ray *r, int *out_i, float *out_t) {
  float best = 1e30f;
  int bi = -1;
  for (int i = 0; i < sc->n; ++i) {
    float t;
    if (hit_sphere(r, &sc->s[i], &t) && t < best) {
      best = t;
      bi = i;
    }
  }
  if (bi < 0) {
    return 0;
  }
  *out_i = bi;
  *out_t = best;
  return 1;
}

/* AABB culling then exact sphere test (ch.8 bounding volumes). */
static int trace_aabb(const Scene *sc, const Ray *r, int *out_i, float *out_t) {
  float best = 1e30f;
  int bi = -1;
  for (int i = 0; i < sc->n; ++i) {
    AABB sb = aabb_sphere(&sc->s[i]);
    float a0, a1;
    if (!hit_aabb(r, &sb, &a0, &a1)) {
      continue;
    }
    float th;
    if (hit_sphere(r, &sc->s[i], &th) && th < best && th >= 1e-3f) {
      best = th;
      bi = i;
    }
  }
  if (bi < 0) {
    return 0;
  }
  *out_i = bi;
  *out_t = best;
  return 1;
}

static double now_ms(void) {
  return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

static Ray pixel_ray(int x, int y, int w, int h, cgl_vec3 eye, cgl_vec3 fwd,
                     cgl_vec3 right, cgl_vec3 up, float half_w, float half_h) {
  float sx = (2.0f * (x + 0.5f) / w - 1.0f) * half_w;
  float sy = (1.0f - 2.0f * (y + 0.5f) / h) * half_h;
  Ray r = {eye, cgl_normalize3(
                    cgl_add3(cgl_add3(fwd, cgl_scale3(right, sx)),
                             cgl_scale3(up, sy)))};
  return r;
}

static int self_test(void) {
  Scene sc;
  sc.n = 2;
  sc.s[0] = (Sph){cgl_v3(0, 0, 0), 1.0f, cgl_v3(1, 0, 0)};
  sc.s[1] = (Sph){cgl_v3(3, 0, 0), 1.0f, cgl_v3(0, 1, 0)};
  scene_bounds(&sc);
  build_grid(&sc);
  Ray r = {cgl_v3(0, 0, 5), cgl_v3(0, 0, -1)};
  int i1 = 0, i2 = 0;
  float t1 = 0, t2 = 0;
  CGL_EXPECT(trace_brute(&sc, &r, &i1, &t1), "brute hit");
  CGL_EXPECT(trace_aabb(&sc, &r, &i2, &t2), "aabb hit");
  CGL_EXPECT(i1 == i2, "same sphere");
  CGL_EXPECT_NEAR(t1, t2, 1e-3, "same t");
  AABB b = {{ -1, -1, -1}, {1, 1, 1}};
  float a, c;
  CGL_EXPECT(hit_aabb(&r, &b, &a, &c), "aabb hit box");
  return cgl_selftest_report("lab08-data-structures");
}

int main(int argc, char **argv) {
  int ns = 120, w = 400, h = 225;
  const char *out = "out/lab08_accel.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--spheres") && i + 1 < argc) {
      ns = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
    }
  }
  if (ns > MAX_SPHERES) ns = MAX_SPHERES;
  if (ns < 1) ns = 1;
  Scene sc;
  sc.n = ns;
  cgl_rng rng;
  cgl_rng_seed(&rng, 3);
  for (int i = 0; i < ns; ++i) {
    sc.s[i].c = cgl_v3(cgl_rng_next01(&rng) * 8 - 4, cgl_rng_next01(&rng) * 4 - 1,
                       cgl_rng_next01(&rng) * 6 - 2);
    sc.s[i].r = 0.15f + 0.35f * cgl_rng_next01(&rng);
    sc.s[i].color = cgl_v3(0.3f + 0.7f * cgl_rng_next01(&rng),
                           0.3f + 0.7f * cgl_rng_next01(&rng),
                           0.3f + 0.7f * cgl_rng_next01(&rng));
  }
  scene_bounds(&sc);
  build_grid(&sc);

  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    return 1;
  }
  cgl_vec3 eye = cgl_v3(0, 1.2f, 8.0f);
  cgl_vec3 fwd = cgl_normalize3(cgl_v3(0, -0.05f, -1));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(fwd, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, fwd);
  float half_h = tanf(45 * CGL_PI / 180 * 0.5f);
  float half_w = half_h * ((float)w / h);
  int mismatch = 0;
  double t0 = now_ms();
  long brute_hits = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      Ray r = pixel_ray(x, y, w, h, eye, fwd, right, up, half_w, half_h);
      int i;
      float t;
      if (trace_brute(&sc, &r, &i, &t)) {
        brute_hits++;
        cgl_vec3 p = cgl_add3(r.o, cgl_scale3(r.d, t));
        cgl_vec3 n = cgl_normalize3(cgl_sub3(p, sc.s[i].c));
        float nd = cgl_clampf(cgl_dot3(n, cgl_normalize3(cgl_v3(-0.3f, -1, -0.2f))), 0, 1);
        cgl_image_set(img, x, y, cgl_add3(cgl_scale3(sc.s[i].color, 0.15f),
                                          cgl_scale3(sc.s[i].color, nd)));
      } else {
        cgl_image_set(img, x, y, cgl_v3(0.08f, 0.09f, 0.12f));
      }
    }
  }
  double t_brute = now_ms() - t0;

  /* time the aabb path alone: any comparison work would pollute the number */
  t0 = now_ms();
  long aabb_hits = 0;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      Ray r = pixel_ray(x, y, w, h, eye, fwd, right, up, half_w, half_h);
      int ia;
      float ta;
      if (trace_aabb(&sc, &r, &ia, &ta)) {
        aabb_hits++;
      }
    }
  }
  double t_aabb = now_ms() - t0;

  /* consistency pass (untimed): brute vs aabb must agree on hit and t */
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      Ray r = pixel_ray(x, y, w, h, eye, fwd, right, up, half_w, half_h);
      int ib, ia;
      float tb, ta;
      int hb = trace_brute(&sc, &r, &ib, &tb);
      int ha = trace_aabb(&sc, &r, &ia, &ta);
      if (hb != ha || (hb && (ib != ia || fabsf(tb - ta) > 1e-3f))) {
        mismatch++;
      }
    }
  }
  if (mismatch) {
    fprintf(stderr, "lab08: %d pixel mismatch between brute and aabb\n", mismatch);
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab08: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab08: wrote %s spheres=%d\n", out, ns);
  printf("lab08: brute %.1f ms (%ld hits), aabb-cull %.1f ms (%ld hits), mismatch=%d\n",
         t_brute, brute_hits, t_aabb, aabb_hits, mismatch);
  cgl_image_free(img);
  return mismatch ? 1 : 0;
}
