#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_color.h"
#include "cgl_image.h"
#include "cgl_selftest.h"
#include "cgl_vec.h"

static int self_test(void) {
  CGL_EXPECT(cgl_linear_to_srgb1(0.18f) > 0.18f, "srgb expands mid grey");
  return cgl_selftest_report("lab12-visual-perception");
}

int main(int argc, char **argv) {
  int w = 512, h = 384;
  const char *out = "out/lab12_perception.ppm";
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
      cgl_vec3 c;
      if (y < h / 3) {
        /* linear grey ramp (display-referred write) */
        float t = (x + 0.5f) / w;
        c = cgl_v3(t, t, t);
      } else if (y < 2 * h / 3) {
        /* sRGB-encoded linear ramp */
        float lin = (x + 0.5f) / w;
        float s = cgl_linear_to_srgb1(lin);
        c = cgl_v3(s, s, s);
      } else {
        /* equal-amplitude sine gratings at three frequencies */
        int band = (x * 3) / w;
        float u = ((x * 3) % w + 0.5f) / w;
        float freq = 4.0f * (float)(1 << band);
        float t = 0.5f + 0.45f * sinf(u * 2.0f * CGL_PI * freq);
        c = cgl_v3(t, t, t);
      }
      cgl_image_set(img, x, y, c);
    }
  }
  if (cgl_image_write_ppm(img, out, 1) != 0) {
    fprintf(stderr, "lab12: failed to write %s\n", out);
    cgl_image_free(img);
    return 1;
  }
  printf("lab12: wrote %s\n", out);
  cgl_image_free(img);
  return 0;
}
