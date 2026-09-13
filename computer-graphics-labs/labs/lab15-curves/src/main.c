#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static inline cgl_vec2 cgl_lerp2(cgl_vec2 a, cgl_vec2 b, float t) {
  return cgl_v2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
}

static cgl_vec2 bezier3(cgl_vec2 p0, cgl_vec2 p1, cgl_vec2 p2, cgl_vec2 p3,
                        float t) {
  float u = 1 - t;
  float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
  return cgl_v2(a * p0.x + b * p1.x + c * p2.x + d * p3.x,
                a * p0.y + b * p1.y + c * p2.y + d * p3.y);
}

static cgl_vec2 de_casteljau(cgl_vec2 p[4], float t) {
  cgl_vec2 a = cgl_lerp2(p[0], p[1], t);
  cgl_vec2 b = cgl_lerp2(p[1], p[2], t);
  cgl_vec2 c = cgl_lerp2(p[2], p[3], t);
  cgl_vec2 d = cgl_lerp2(a, b, t);
  cgl_vec2 e = cgl_lerp2(b, c, t);
  return cgl_lerp2(d, e, t);
}

/* B'(t) = 3u^2(P1-P0) + 6ut(P2-P1) + 3t^2(P3-P2) */
static cgl_vec2 bezier3_d1(cgl_vec2 p0, cgl_vec2 p1, cgl_vec2 p2, cgl_vec2 p3,
                           float t) {
  float u = 1 - t;
  return cgl_v2(3 * u * u * (p1.x - p0.x) + 6 * u * t * (p2.x - p1.x) +
                    3 * t * t * (p3.x - p2.x),
                3 * u * u * (p1.y - p0.y) + 6 * u * t * (p2.y - p1.y) +
                    3 * t * t * (p3.y - p2.y));
}

/* B''(t) = 6u(P2-2P1+P0) + 6t(P3-2P2+P1) */
static cgl_vec2 bezier3_d2(cgl_vec2 p0, cgl_vec2 p1, cgl_vec2 p2, cgl_vec2 p3,
                           float t) {
  float u = 1 - t;
  return cgl_v2(6 * u * (p2.x - 2 * p1.x + p0.x) + 6 * t * (p3.x - 2 * p2.x + p1.x),
                6 * u * (p2.y - 2 * p1.y + p0.y) + 6 * t * (p3.y - 2 * p2.y + p1.y));
}

/* Signed curvature kappa = (x'y'' - y'x'') / (x'^2 + y'^2)^{3/2} */
static float curvature(cgl_vec2 p0, cgl_vec2 p1, cgl_vec2 p2, cgl_vec2 p3,
                       float t) {
  cgl_vec2 d1 = bezier3_d1(p0, p1, p2, p3, t);
  cgl_vec2 d2 = bezier3_d2(p0, p1, p2, p3, t);
  float denom = powf(d1.x * d1.x + d1.y * d1.y, 1.5f);
  if (denom < 1e-9f) {
    return 0.0f;
  }
  return (d1.x * d2.y - d1.y * d2.x) / denom;
}

static void draw_disk(cgl_image *img, float x, float y, float rad, cgl_vec3 col) {
  int x0 = (int)floorf(x - rad - 1), x1 = (int)ceilf(x + rad + 1);
  int y0 = (int)floorf(y - rad - 1), y1 = (int)ceilf(y + rad + 1);
  for (int j = y0; j <= y1; ++j) {
    for (int i = x0; i <= x1; ++i) {
      float dx = i + 0.5f - x, dy = j + 0.5f - y;
      if (dx * dx + dy * dy <= rad * rad) {
        cgl_image_set(img, i, j, col);
      }
    }
  }
}

static void draw_line(cgl_image *img, float x0, float y0, float x1, float y1,
                      cgl_vec3 col) {
  int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) * 2) + 1;
  for (int i = 0; i <= n; ++i) {
    float t = (float)i / (float)n;
    draw_disk(img, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.8f, col);
  }
}

static int self_test(void) {
  cgl_vec2 p0 = cgl_v2(0, 0), p1 = cgl_v2(0, 1), p2 = cgl_v2(1, 1),
           p3 = cgl_v2(1, 0);
  CGL_EXPECT_NEAR(bezier3(p0, p1, p2, p3, 0).x, 0, 1e-5, "t0");
  CGL_EXPECT_NEAR(bezier3(p0, p1, p2, p3, 1).x, 1, 1e-5, "t1");
  cgl_vec2 pts[4] = {p0, p1, p2, p3};
  for (int i = 0; i <= 8; ++i) {
    float t = i / 8.0f;
    cgl_vec2 a = bezier3(p0, p1, p2, p3, t);
    cgl_vec2 b = de_casteljau(pts, t);
    CGL_EXPECT_NEAR(a.x, b.x, 1e-5, "dc x");
    CGL_EXPECT_NEAR(a.y, b.y, 1e-5, "dc y");
  }
  /* quarter-circle Bezier (k = 4/3*(sqrt2-1)) has |kappa(0.5)| ~ 1/radius = 1 */
  float kk = 0.5522847498f;
  float kc = curvature(cgl_v2(1, 0), cgl_v2(1, kk), cgl_v2(kk, 1),
                       cgl_v2(0, 1), 0.5f);
  CGL_EXPECT_NEAR(fabsf(kc), 1.0f, 0.05f, "quarter circle curvature ~ 1");
  float kl = curvature(cgl_v2(0, 0), cgl_v2(1, 1), cgl_v2(2, 2), cgl_v2(3, 3),
                       0.3f);
  CGL_EXPECT_NEAR(kl, 0.0f, 1e-6f, "straight line curvature is 0");
  return cgl_selftest_report("lab15-curves");
}

int main(int argc, char **argv) {
  int w = 640, h = 400;
  const char *out = "out/lab15_curves.ppm";
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
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      cgl_image_set(img, x, y, cgl_v3(0.07f, 0.08f, 0.1f));
    }
  }
  cgl_vec2 p[4] = {cgl_v2(80, 320), cgl_v2(160, 60), cgl_v2(480, 60),
                   cgl_v2(560, 320)};
  draw_line(img, p[0].x, p[0].y, p[1].x, p[1].y, cgl_v3(0.4f, 0.4f, 0.45f));
  draw_line(img, p[1].x, p[1].y, p[2].x, p[2].y, cgl_v3(0.4f, 0.4f, 0.45f));
  draw_line(img, p[2].x, p[2].y, p[3].x, p[3].y, cgl_v3(0.4f, 0.4f, 0.45f));
  /* first pass: |kappa| range along the curve for color/width normalization */
  float kmax = 0;
  for (int i = 0; i <= 400; ++i) {
    float t = i / 400.0f;
    float ka = fabsf(curvature(p[0], p[1], p[2], p[3], t));
    if (ka > kmax) {
      kmax = ka;
    }
  }
  for (int i = 0; i <= 400; ++i) {
    float t = i / 400.0f;
    cgl_vec2 q = bezier3(p[0], p[1], p[2], p[3], t);
    /* curvature-mapped stroke: blue (flat) -> red (curvy), wider = curvier */
    float s = kmax > 0 ? fabsf(curvature(p[0], p[1], p[2], p[3], t)) / kmax : 0;
    cgl_vec3 col = cgl_lerp3(cgl_v3(0.35f, 0.55f, 0.95f), cgl_v3(0.95f, 0.3f, 0.2f), s);
    draw_disk(img, q.x, q.y, 1.0f + 2.8f * s, col);
    if (i % 40 == 0) {
      cgl_vec2 d1 = bezier3_d1(p[0], p[1], p[2], p[3], t);
      float len = sqrtf(d1.x * d1.x + d1.y * d1.y) + 1e-6f;
      draw_line(img, q.x, q.y, q.x + d1.x / len * 28, q.y + d1.y / len * 28,
                cgl_v3(0.3f, 0.85f, 0.9f));
      draw_disk(img, q.x, q.y, 3.0f, cgl_v3(0.9f, 0.3f, 0.3f));
    }
  }
  draw_disk(img, p[0].x, p[0].y, 5, cgl_v3(0.9f, 0.3f, 0.3f));
  draw_disk(img, p[1].x, p[1].y, 5, cgl_v3(0.3f, 0.8f, 0.4f));
  draw_disk(img, p[2].x, p[2].y, 5, cgl_v3(0.3f, 0.8f, 0.4f));
  draw_disk(img, p[3].x, p[3].y, 5, cgl_v3(0.9f, 0.3f, 0.3f));
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab15: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab15: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
