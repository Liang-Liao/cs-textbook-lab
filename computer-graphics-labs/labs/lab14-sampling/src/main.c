#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_rng.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

#define N 64         /* strata per axis, N*N samples per batch */
#define BATCHES 64   /* independent batches used to estimate Var(I_hat) */

static void draw_points(cgl_image *img, int x0, int y0, int w, int h,
                        const float *xs, const float *ys, int n) {
  for (int y = y0; y < y0 + h; ++y) {
    for (int x = x0; x < x0 + w; ++x) {
      cgl_image_set(img, x, y, cgl_v3(0.08f, 0.09f, 0.12f));
    }
  }
  for (int i = 0; i < n; ++i) {
    int px = x0 + (int)(xs[i] * (w - 1));
    int py = y0 + (int)(ys[i] * (h - 1));
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        int xx = px + dx, yy = py + dy;
        if (xx >= x0 && yy >= y0 && xx < x0 + w && yy < y0 + h) {
          cgl_image_set(img, xx, yy, cgl_v3(0.9f, 0.85f, 0.4f));
        }
      }
    }
  }
}

static void sample_random(float *xs, float *ys, cgl_rng *rng) {
  for (int i = 0; i < N * N; ++i) {
    xs[i] = cgl_rng_next01(rng);
    ys[i] = cgl_rng_next01(rng);
  }
}

static void sample_grid(float *xs, float *ys) {
  int k = 0;
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      xs[k] = (i + 0.5f) / N;
      ys[k] = (j + 0.5f) / N;
      k++;
    }
  }
}

static void sample_stratified(float *xs, float *ys, cgl_rng *rng) {
  int k = 0;
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      xs[k] = (i + cgl_rng_next01(rng)) / N;
      ys[k] = (j + cgl_rng_next01(rng)) / N;
      k++;
    }
  }
}

static void sample_jittered(float *xs, float *ys, cgl_rng *rng) {
  int k = 0;
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      xs[k] = (i + 0.5f + (cgl_rng_next01(rng) - 0.5f) * 0.8f) / N;
      ys[k] = (j + 0.5f + (cgl_rng_next01(rng) - 0.5f) * 0.8f) / N;
      if (xs[k] < 0) xs[k] = 0;
      if (ys[k] < 0) ys[k] = 0;
      if (xs[k] > 1) xs[k] = 1;
      if (ys[k] > 1) ys[k] = 1;
      k++;
    }
  }
}

/* ---- Estimator variance for I = integral of f(x,y)=x*y over [0,1]^2 ----
 * Var(I_hat) is estimated from BATCHES independent batch means:
 *   Var(I_hat) ~= (1/B) * sum_b (I_b - mean_I)^2
 * This is the discriminating metric: the pooled sample variance
 * E[v^2]-E[v]^2 of the points themselves is ~7/144 for BOTH random and
 * grid sampling of x*y and cannot tell the methods apart. */

static float mean_from_points(const float *xs, const float *ys) {
  double s = 0;
  for (int i = 0; i < N * N; ++i) {
    s += (double)xs[i] * ys[i];
  }
  return (float)(s / (N * N));
}

/* batch variance + overall mean of the batch means */
static void batch_stats(const float *means, int b, float *var_out,
                        float *mean_out) {
  double s = 0, s2 = 0;
  for (int i = 0; i < b; ++i) {
    s += means[i];
    s2 += (double)means[i] * means[i];
  }
  s /= b;
  s2 /= b;
  *var_out = (float)(s2 - s * s);
  *mean_out = (float)s;
}

/* Bar chart of the four methods' estimator variance, normalized to the max. */
static void draw_bars(cgl_image *img, const float *v, int n, int w, int h) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      cgl_image_set(img, x, y, cgl_v3(0.08f, 0.09f, 0.12f));
    }
  }
  static const cgl_vec3 colors[4] = {
      {0.9f, 0.85f, 0.4f}, {0.4f, 0.8f, 0.5f}, {0.4f, 0.6f, 0.95f},
      {0.9f, 0.5f, 0.3f}};
  float vmax = v[0];
  for (int i = 1; i < n; ++i) {
    if (v[i] > vmax) vmax = v[i];
  }
  int bw = 56, gap = 24;
  int x = (w - n * bw - (n - 1) * gap) / 2;
  int base = h - 24;
  for (int i = 0; i < n; ++i) {
    int bh = vmax > 0 ? (int)(v[i] / vmax * (float)(base - 20)) : 0;
    if (v[i] > 0 && bh < 1) bh = 1;
    for (int y = base - bh; y < base; ++y) {
      for (int xx = x; xx < x + bw; ++xx) {
        cgl_image_set(img, xx, y, colors[i % 4]);
      }
    }
    x += bw + gap;
  }
  for (int xx = 8; xx < w - 8; ++xx) {
    cgl_image_set(img, xx, base, cgl_v3(0.7f, 0.7f, 0.75f));
  }
}

static int self_test(void) {
  cgl_rng rng;
  cgl_rng_seed(&rng, 5);
  float means[BATCHES];
  float xs[N * N], ys[N * N];

  for (int b = 0; b < BATCHES; ++b) {
    sample_random(xs, ys, &rng);
    means[b] = mean_from_points(xs, ys);
  }
  float vr, mr;
  batch_stats(means, BATCHES, &vr, &mr);

  sample_grid(xs, ys);
  means[0] = mean_from_points(xs, ys);
  float vg, mg;
  batch_stats(means, 1, &vg, &mg); /* deterministic: all batches identical */

  for (int b = 0; b < BATCHES; ++b) {
    sample_stratified(xs, ys, &rng);
    means[b] = mean_from_points(xs, ys);
  }
  float vst, mst;
  batch_stats(means, BATCHES, &vst, &mst);

  CGL_EXPECT(vr > 0, "random estimator variance positive");
  CGL_EXPECT(vg == 0, "grid quadrature is deterministic (zero variance)");
  CGL_EXPECT(vst < vr, "stratified estimator variance < random");
  CGL_EXPECT(fabsf(mr - 0.25f) < 0.02f, "random MC estimate near 1/4");
  return cgl_selftest_report("lab14-sampling");
}

int main(int argc, char **argv) {
  const char *out = "out/lab14_sampling.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    }
  }
  int cell = 180;
  int w = cell * 2, h = cell * 2;
  cgl_image *img = cgl_image_create(w, h);
  if (!img) {
    fprintf(stderr, "lab14: image create failed (%dx%d)\n", w, h);
    return 1;
  }
  float xs[N * N], ys[N * N];
  float means[BATCHES];
  cgl_rng rng;
  cgl_rng_seed(&rng, 9);

  sample_random(xs, ys, &rng);
  draw_points(img, 0, 0, cell, cell, xs, ys, N * N);
  for (int b = 0; b < BATCHES; ++b) {
    sample_random(xs, ys, &rng);
    means[b] = mean_from_points(xs, ys);
  }
  float vr, mr;
  batch_stats(means, BATCHES, &vr, &mr);

  sample_grid(xs, ys);
  draw_points(img, cell, 0, cell, cell, xs, ys, N * N);
  means[0] = mean_from_points(xs, ys);
  float vg, mg;
  batch_stats(means, 1, &vg, &mg);

  sample_stratified(xs, ys, &rng);
  draw_points(img, 0, cell, cell, cell, xs, ys, N * N);
  for (int b = 0; b < BATCHES; ++b) {
    sample_stratified(xs, ys, &rng);
    means[b] = mean_from_points(xs, ys);
  }
  float vs, ms;
  batch_stats(means, BATCHES, &vs, &ms);

  sample_jittered(xs, ys, &rng);
  draw_points(img, cell, cell, cell, cell, xs, ys, N * N);
  for (int b = 0; b < BATCHES; ++b) {
    sample_jittered(xs, ys, &rng);
    means[b] = mean_from_points(xs, ys);
  }
  float vj, mj;
  batch_stats(means, BATCHES, &vj, &mj);

  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab14: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  /* estimator-variance comparison chart, same method order as the panels */
  char varout[256];
  snprintf(varout, sizeof varout, "%s", out);
  char *dot = strrchr(varout, '.');
  if (dot) {
    snprintf(dot, sizeof varout - (size_t)(dot - varout), "_variance.ppm");
  }
  int vw = 360, vh = 220;
  cgl_image *vimg = cgl_image_create(vw, vh);
  if (!vimg) {
    fprintf(stderr, "lab14: variance image create failed\n");
    cgl_image_free(img);
    return 1;
  }
  float vars[4] = {vr, vg, vs, vj};
  draw_bars(vimg, vars, 4, vw, vh);
  int rc = 0;
  if (cgl_image_write_ppm_srgb(vimg, varout) != 0) {
    fprintf(stderr, "lab14: failed to write %s\n", varout);
    rc = 1;
  }
  cgl_image_free(vimg);
  cgl_image_free(img);
  if (rc) {
    return 1;
  }
  printf("lab14: wrote %s and %s\n", out, varout);
  printf("lab14: Var(I_hat) random=%.3e grid=%.3e stratified=%.3e jittered=%.3e (batches=%d, n=%d)\n",
         vr, vg, vs, vj, BATCHES, N * N);
  printf("lab14: I_hat vs truth 0.25: random=%.4f grid=%.4f stratified=%.4f jittered=%.4f\n",
         mr, mg, ms, mj);
  return 0;
}
