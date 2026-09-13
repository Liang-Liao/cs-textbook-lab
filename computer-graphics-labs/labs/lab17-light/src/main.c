#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_rng.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

typedef struct {
  cgl_vec3 pos;
  cgl_vec3 color;
  float intensity;
} PointLight;

/* Rectangular area light: center + two orthogonal unit directions + half extent */
typedef struct {
  cgl_vec3 pos;
  cgl_vec3 right, up;
  float half;
  cgl_vec3 color;
  float intensity;
} AreaLight;

typedef struct {
  cgl_vec3 c;
  float r;
  cgl_vec3 albedo;
} Sph;

static float smoothstepf(float a, float b, float x) {
  float t = cgl_clampf((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

static cgl_vec3 point_light(const PointLight *L, cgl_vec3 p, cgl_vec3 n) {
  cgl_vec3 d = cgl_sub3(L->pos, p);
  float dist2 = cgl_len2_3(d);
  cgl_vec3 wi = cgl_normalize3(d);
  float ndotl = cgl_clampf(cgl_dot3(n, wi), 0, 1);
  float att = L->intensity / (1.0f + 0.09f * dist2);
  return cgl_scale3(L->color, ndotl * att);
}

static cgl_vec3 spot_light(const PointLight *L, cgl_vec3 dir, float cos_inner,
                           float cos_outer, cgl_vec3 p, cgl_vec3 n) {
  cgl_vec3 d = cgl_sub3(L->pos, p);
  cgl_vec3 wi = cgl_normalize3(d);
  float cosang = cgl_dot3(cgl_neg3(wi), cgl_normalize3(dir));
  float mask = smoothstepf(cos_outer, cos_inner, cosang);
  cgl_vec3 base = point_light(L, p, n);
  return cgl_scale3(base, mask);
}

static cgl_vec3 dir_light(cgl_vec3 dir, cgl_vec3 color, float intensity,
                          cgl_vec3 n) {
  cgl_vec3 wi = cgl_normalize3(cgl_neg3(dir));
  float ndotl = cgl_clampf(cgl_dot3(n, wi), 0, 1);
  return cgl_scale3(color, ndotl * intensity);
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

/* Is the segment p->l blocked by any sphere (ground cannot occlude here)? */
static int occluded(cgl_vec3 p, cgl_vec3 l, const Sph *spheres, int ns) {
  cgl_vec3 d = cgl_sub3(l, p);
  float dist = cgl_len3(d);
  if (dist < 1e-6f) {
    return 0;
  }
  cgl_vec3 rd = cgl_scale3(d, 1.0f / dist);
  for (int i = 0; i < ns; ++i) {
    float t;
    cgl_vec3 n;
    if (hit_sphere(p, rd, spheres[i].c, spheres[i].r, &t, &n) &&
        t < dist - 1e-3f) {
      return 1;
    }
  }
  return 0;
}

/* Uniformly sample spp points on the light rectangle; each sample carries its
 * own visibility, so partially blocked points land in the penumbra. */
static cgl_vec3 area_light_sampled(const AreaLight *A, cgl_vec3 p, cgl_vec3 n,
                                   const Sph *spheres, int ns, cgl_rng *rng,
                                   int spp) {
  cgl_vec3 sum = cgl_v3(0, 0, 0);
  for (int s = 0; s < spp; ++s) {
    float u = cgl_rng_next01(rng) * 2.0f - 1.0f;
    float v = cgl_rng_next01(rng) * 2.0f - 1.0f;
    cgl_vec3 lp = cgl_add3(A->pos, cgl_add3(cgl_scale3(A->right, u * A->half),
                                            cgl_scale3(A->up, v * A->half)));
    cgl_vec3 d = cgl_sub3(lp, p);
    float dist2 = cgl_len2_3(d);
    cgl_vec3 wi = cgl_normalize3(d);
    float ndotl = cgl_clampf(cgl_dot3(n, wi), 0, 1);
    if (ndotl <= 0.0f || occluded(p, lp, spheres, ns)) {
      continue;
    }
    float att = A->intensity / (1.0f + 0.09f * dist2);
    sum = cgl_add3(sum, cgl_scale3(A->color, ndotl * att));
  }
  return cgl_scale3(sum, 1.0f / (float)spp);
}

static int self_test(void) {
  PointLight L = {cgl_v3(0, 2, 0), cgl_v3(1, 1, 1), 4.0f};
  cgl_vec3 n = cgl_v3(0, 1, 0);
  cgl_vec3 a = point_light(&L, cgl_v3(0, 0, 0), n);
  cgl_vec3 b = point_light(&L, cgl_v3(0, 0, 3), n);
  CGL_EXPECT(cgl_len3(a) > cgl_len3(b), "attenuation with distance");
  cgl_vec3 s = spot_light(&L, cgl_v3(0, -1, 0), 0.9f, 0.7f, cgl_v3(0, 0, 0), n);
  cgl_vec3 s2 = spot_light(&L, cgl_v3(0, -1, 0), 0.9f, 0.7f, cgl_v3(2, 0, 0), n);
  CGL_EXPECT(cgl_len3(s) > cgl_len3(s2), "spot cone falloff");

  Sph spheres[1] = {{cgl_v3(0.5f, 0, 0), 0.4f, cgl_v3(1, 1, 1)}};
  AreaLight A = {cgl_v3(0, 2, 0), cgl_v3(1, 0, 0), cgl_v3(0, 0, 1), 0.5f,
                 cgl_v3(1, 1, 1), 4.0f};
  cgl_rng rng;
  cgl_rng_seed(&rng, 5);
  cgl_vec3 a0 = area_light_sampled(&A, cgl_v3(0, 0, 0), n, spheres, 1, &rng, 64);
  CGL_EXPECT(cgl_len3(a0) > 0.0f, "area light lights point below center");
  cgl_vec3 a1 = area_light_sampled(&A, cgl_v3(3, 0, 0), n, spheres, 1, &rng, 64);
  CGL_EXPECT(cgl_len3(a0) > cgl_len3(a1), "area light falls off with distance");
  CGL_EXPECT(occluded(cgl_v3(-1.5f, 0, 0), cgl_v3(2.5f, 0, 0), spheres, 1),
             "segment through sphere is occluded");
  CGL_EXPECT(!occluded(cgl_v3(0, 1, 0), cgl_v3(0, 3, 0), spheres, 1),
             "clear segment is visible");
  return cgl_selftest_report("lab17-light");
}

int main(int argc, char **argv) {
  int w = 640, h = 240;
  int spp = 32;
  unsigned long long seed = 7;
  const char *out = "out/lab17_light.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    } else if (!strcmp(argv[i], "--spp") && i + 1 < argc) {
      spp = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
      seed = strtoull(argv[++i], NULL, 10);
    }
  }
  if (spp < 1) spp = 1;
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    fprintf(stderr, "lab17: image create failed (%dx%d)\n", w, h);
    return 1;
  }
  Sph spheres[4] = {
      {cgl_v3(-2.2f, 0, 0), 0.85f, cgl_v3(0.8f, 0.45f, 0.25f)},
      {cgl_v3(-0.7f, 0, 0), 0.85f, cgl_v3(0.8f, 0.45f, 0.25f)},
      {cgl_v3(0.8f, 0, 0), 0.85f, cgl_v3(0.8f, 0.45f, 0.25f)},
      {cgl_v3(2.3f, 0, 0), 0.85f, cgl_v3(0.8f, 0.45f, 0.25f)},
  };
  cgl_vec3 eye = cgl_v3(0, 1.6f, 6.5f);
  PointLight pt = {cgl_v3(-2.2f, 2.5f, 1.5f), cgl_v3(1.0f, 0.9f, 0.75f), 5.0f};
  PointLight sp = {cgl_v3(0.8f, 3.0f, 1.2f), cgl_v3(0.6f, 0.8f, 1.0f), 8.0f};
  cgl_vec3 spot_dir = cgl_normalize3(cgl_v3(0.1f, -1.0f, -0.15f));
  AreaLight area = {cgl_v3(2.3f, 2.8f, 0.8f), cgl_v3(1, 0, 0), cgl_v3(0, 0, 1),
                    0.7f, cgl_v3(1.0f, 0.85f, 0.6f), 4.0f};
  cgl_rng rng;
  cgl_rng_seed(&rng, seed);
  cgl_vec3 fwd = cgl_normalize3(cgl_sub3(cgl_v3(0, 0.2f, 0), eye));
  cgl_vec3 right = cgl_normalize3(cgl_cross3(fwd, cgl_v3(0, 1, 0)));
  cgl_vec3 up = cgl_cross3(right, fwd);
  float half_h = tanf(45 * CGL_PI / 180 * 0.5f);
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
      for (int i = 0; i < 4; ++i) {
        float t;
        cgl_vec3 n;
        if (hit_sphere(eye, rd, spheres[i].c, spheres[i].r, &t, &n) &&
            t < best) {
          best = t;
          bi = i;
          bn = n;
        }
      }
      float tplane;
      if (fabsf(rd.y) > 1e-8f) {
        tplane = (-1.0f - eye.y) / rd.y;
        if (tplane > 1e-3f && tplane < best) {
          best = tplane;
          bi = 100;
          bn = cgl_v3(0, 1, 0);
        }
      }
      cgl_vec3 col = cgl_v3(0.06f, 0.07f, 0.1f);
      if (bi >= 0) {
        cgl_vec3 p = cgl_add3(eye, cgl_scale3(rd, best));
        cgl_vec3 alb = bi == 100 ? cgl_v3(0.55f, 0.55f, 0.58f) : spheres[bi].albedo;
        cgl_vec3 L = cgl_scale3(alb, 0.05f);
        /* delta lights: one visibility test -> hard shadows */
        if (!occluded(p, pt.pos, spheres, 4)) {
          L = cgl_add3(L, cgl_mul3(alb, point_light(&pt, p, bn)));
        }
        if (!occluded(p, sp.pos, spheres, 4)) {
          L = cgl_add3(L, cgl_mul3(alb, spot_light(&sp, spot_dir, 0.95f, 0.75f,
                                                   p, bn)));
        }
        L = cgl_add3(L, cgl_mul3(alb, dir_light(cgl_v3(-0.3f, -1, -0.2f),
                                                cgl_v3(0.35f, 0.4f, 0.5f), 0.25f, bn)));
        /* area light: averaged samples -> soft shadows */
        L = cgl_add3(L, cgl_mul3(alb, area_light_sampled(&area, p, bn, spheres,
                                                         4, &rng, spp)));
        col = L;
      }
      cgl_image_set(img, x, y, col);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab17: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab17: wrote %s (spp=%d seed=%llu)\n", out, spp, seed);
  cgl_image_free(img);
  return 0;
}
