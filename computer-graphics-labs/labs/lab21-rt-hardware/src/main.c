#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgl_selftest.h"
#include "cgl_vec.h"

static int self_test(void) {
  cgl_mat4 P = cgl_perspective(CGL_PI / 3, 1.0f, 0.1f, 100.0f);
  cgl_vec4 clip = cgl_mat4_mulv(&P, cgl_v4(0, 0, -2, 1));
  CGL_EXPECT(fabsf(clip.w) > 1e-6, "clip w");
  cgl_vec3 ndc = cgl_v3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
  CGL_EXPECT(ndc.z >= -1.0f && ndc.z <= 1.0f, "ndc z range");
  return cgl_selftest_report("lab21-rt-hardware");
}

int main(int argc, char **argv) {
  const char *out = "out/lab21_pipeline.txt";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--self-test")) {
      return self_test();
    } else if (!strcmp(argv[i], "--out") && i + 1 < argc) {
      out = argv[++i];
    }
  }
  FILE *f = fopen(out, "w");
  if (!f) {
    fprintf(stderr, "cannot write %s\n", out);
    return 1;
  }
  cgl_mat4 M = cgl_translate(cgl_v3(0.2f, 0, 0));
  cgl_mat4 V = cgl_look_at(cgl_v3(0, 1, 4), cgl_v3(0, 0, 0), cgl_v3(0, 1, 0));
  cgl_mat4 P = cgl_perspective(50.0f * CGL_PI / 180.0f, 16.0f / 9.0f, 0.1f, 50.0f);
  cgl_mat4 MV = cgl_mat4_mul(&V, &M);
  cgl_mat4 MVP = cgl_mat4_mul(&P, &MV);
  fprintf(f, "=== software pipeline stages (lab21) ===\n");
  cgl_vec3 tri[3] = {cgl_v3(-1, 0, 0), cgl_v3(1, 0, 0), cgl_v3(0, 1.2f, 0)};
  fprintf(f, "1) object-space vertices:\n");
  for (int i = 0; i < 3; ++i) {
    fprintf(f, "  v%d = (%.3f, %.3f, %.3f)\n", i, tri[i].x, tri[i].y, tri[i].z);
  }
  fprintf(f, "2) clip-space (MVP * v):\n");
  cgl_vec4 clip[3];
  for (int i = 0; i < 3; ++i) {
    clip[i] = cgl_mat4_mulv(&MVP, cgl_v4(tri[i].x, tri[i].y, tri[i].z, 1));
    fprintf(f, "  c%d = (%.3f, %.3f, %.3f, %.3f)\n", i, clip[i].x, clip[i].y,
            clip[i].z, clip[i].w);
  }
  fprintf(f, "3) NDC (perspective divide):\n");
  for (int i = 0; i < 3; ++i) {
    float w = clip[i].w != 0 ? clip[i].w : 1e-8f;
    fprintf(f, "  n%d = (%.3f, %.3f, %.3f)\n", i, clip[i].x / w, clip[i].y / w,
            clip[i].z / w);
  }
  int wpx = 1920, hpx = 1080;
  fprintf(f, "4) viewport transform to screen (%dx%d):\n", wpx, hpx);
  for (int i = 0; i < 3; ++i) {
    float w = clip[i].w != 0 ? clip[i].w : 1e-8f;
    float nx = clip[i].x / w, ny = clip[i].y / w;
    float sx = (nx * 0.5f + 0.5f) * wpx;
    float sy = (1.0f - (ny * 0.5f + 0.5f)) * hpx; /* y-down screen */
    fprintf(f, "  s%d = (%.1f, %.1f)\n", i, sx, sy);
  }
  fprintf(f, "5) primitive assembly: triangle (v0,v1,v2)\n");
  fprintf(f, "6) rasterizer would cover pixels via edge equations (see lab22)\n");
  fprintf(f, "7) fragment shader stages: interpolate attributes, shade, z-test\n");
  fclose(f);
  printf("lab21: wrote %s\n", out);
  return 0;
}
