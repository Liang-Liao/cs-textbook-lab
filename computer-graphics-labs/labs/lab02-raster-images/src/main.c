#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static float quantize(float v, int levels) {
  if (levels < 2) {
    levels = 2;
  }
  float t = cgl_clampf(v, 0.0f, 1.0f) * (float)(levels - 1);
  return floorf(t + 0.5f) / (float)(levels - 1);
}

static cgl_vec3 field_color(float u, float v, float freq, int quant_levels) {
  int cx = (int)floorf(u * freq);
  int cy = (int)floorf(v * freq);
  float checker = ((cx + cy) & 1) ? 1.0f : 0.0f;
  float grad = 0.5f + 0.5f * sinf(u * 6.2831853f);
  cgl_vec3 c = cgl_v3(checker * grad, grad * 0.6f + 0.2f, 1.0f - checker);
  if (quant_levels > 0) {
    c = cgl_v3(quantize(c.x, quant_levels), quantize(c.y, quant_levels),
               quantize(c.z, quant_levels));
  }
  return c;
}

/* Box-filter downsample of analytic field, then nearest upsample. */
static cgl_vec3 filtered_quad(float u, float v, float freq, int down) {
  int samples = down * down;
  cgl_vec3 acc = cgl_v3(0, 0, 0);
  for (int j = 0; j < down; ++j) {
    for (int i = 0; i < down; ++i) {
      float cu = (floorf(u * 64.0f) + (i + 0.5f)) / 64.0f;
      float cv = (floorf(v * 64.0f) + (j + 0.5f)) / 64.0f;
      acc = cgl_add3(acc, field_color(cu, cv, freq, 0));
    }
  }
  return cgl_scale3(acc, 1.0f / (float)samples);
}

static int self_test(void) {
  CGL_EXPECT_NEAR(quantize(0.0f, 2), 0.0f, 1e-6, "quant 0");
  CGL_EXPECT_NEAR(quantize(1.0f, 2), 1.0f, 1e-6, "quant 1");
  CGL_EXPECT_NEAR(quantize(0.4f, 2), 0.0f, 1e-6, "quant mid->0");
  CGL_EXPECT_NEAR(quantize(0.6f, 2), 1.0f, 1e-6, "quant mid->1");
  return cgl_selftest_report("lab02-raster-images");
}

int main(int argc, char **argv) {
  int w = 512, h = 512;
  const char *out = "out/lab02_sampling.ppm";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
      w = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--height") && i + 1 < argc) {
      h = atoi(argv[++i]);
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
      float u = (x + 0.5f) / (float)w;
      float v = (y + 0.5f) / (float)h;
      cgl_vec3 c;
      if (u < 0.5f && v < 0.5f) {
        c = field_color(u * 2, v * 2, 32.0f, 0);
      } else if (u >= 0.5f && v < 0.5f) {
        c = filtered_quad((u - 0.5f) * 2, v * 2, 32.0f, 4);
      } else if (u < 0.5f && v >= 0.5f) {
        c = field_color(u * 2, (v - 0.5f) * 2, 12.0f, 8);
      } else {
        c = field_color((u - 0.5f) * 2, (v - 0.5f) * 2, 12.0f, 16);
      }
      cgl_image_set(img, x, y, c);
    }
  }
  if (cgl_image_write_ppm_srgb(img, out) != 0) {
    fprintf(stderr, "lab02: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab02: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
