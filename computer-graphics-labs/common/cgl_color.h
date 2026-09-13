#ifndef CGL_COLOR_H
#define CGL_COLOR_H

#include "cgl_vec.h"

static inline float cgl_linear_to_srgb1(float x) {
  x = cgl_clampf(x, 0.0f, 1.0f);
  if (x <= 0.0031308f) {
    return 12.92f * x;
  }
  return 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

static inline float cgl_srgb_to_linear1(float x) {
  x = cgl_clampf(x, 0.0f, 1.0f);
  if (x <= 0.04045f) {
    return x / 12.92f;
  }
  return powf((x + 0.055f) / 1.055f, 2.4f);
}

static inline cgl_vec3 cgl_linear_to_srgb(cgl_vec3 c) {
  return cgl_v3(cgl_linear_to_srgb1(c.x), cgl_linear_to_srgb1(c.y),
                cgl_linear_to_srgb1(c.z));
}

static inline cgl_vec3 cgl_srgb_to_linear(cgl_vec3 c) {
  return cgl_v3(cgl_srgb_to_linear1(c.x), cgl_srgb_to_linear1(c.y),
                cgl_srgb_to_linear1(c.z));
}

static inline cgl_vec3 cgl_gamma_encode(cgl_vec3 c, float gamma) {
  float inv = 1.0f / gamma;
  c = cgl_clamp3(c, 0.0f, 1.0f);
  return cgl_v3(powf(c.x, inv), powf(c.y, inv), powf(c.z, inv));
}

#endif /* CGL_COLOR_H */
