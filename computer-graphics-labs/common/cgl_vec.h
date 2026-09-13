#ifndef CGL_VEC_H
#define CGL_VEC_H

#include <math.h>
#include <stdbool.h>

#include "cgl_assert.h"

/* Column-major mat4: m[col * 4 + row], applied as M * v. */
typedef struct {
  float x, y;
} cgl_vec2;

typedef struct {
  float x, y, z;
} cgl_vec3;

typedef struct {
  float x, y, z, w;
} cgl_vec4;

typedef struct {
  float m[16];
} cgl_mat4;

#ifndef CGL_PI
#define CGL_PI 3.14159265358979323846f
#endif

static inline float cgl_clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

static inline float cgl_lerpf(float a, float b, float t) { return a + (b - a) * t; }

static inline cgl_vec2 cgl_v2(float x, float y) {
  cgl_vec2 v = {x, y};
  return v;
}

static inline cgl_vec3 cgl_v3(float x, float y, float z) {
  cgl_vec3 v = {x, y, z};
  return v;
}

static inline cgl_vec4 cgl_v4(float x, float y, float z, float w) {
  cgl_vec4 v = {x, y, z, w};
  return v;
}

static inline cgl_vec3 cgl_v3s(float s) { return cgl_v3(s, s, s); }

static inline cgl_vec3 cgl_add3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline cgl_vec3 cgl_sub3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline cgl_vec3 cgl_mul3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(a.x * b.x, a.y * b.y, a.z * b.z);
}

static inline cgl_vec3 cgl_div3(cgl_vec3 a, cgl_vec3 b) {
  CGL_CHECK(b.x != 0.0f && b.y != 0.0f && b.z != 0.0f, "cgl_div3: zero divisor");
  return cgl_v3(a.x / b.x, a.y / b.y, a.z / b.z);
}

static inline cgl_vec3 cgl_scale3(cgl_vec3 a, float s) {
  return cgl_v3(a.x * s, a.y * s, a.z * s);
}

static inline cgl_vec3 cgl_neg3(cgl_vec3 a) { return cgl_v3(-a.x, -a.y, -a.z); }

static inline float cgl_dot3(cgl_vec3 a, cgl_vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline cgl_vec3 cgl_cross3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

static inline float cgl_len2_3(cgl_vec3 a) { return cgl_dot3(a, a); }

static inline float cgl_len3(cgl_vec3 a) { return sqrtf(cgl_len2_3(a)); }

static inline cgl_vec3 cgl_normalize3(cgl_vec3 a) {
  float len = cgl_len3(a);
  if (len <= 1e-20f) {
    return cgl_v3(0.0f, 0.0f, 0.0f);
  }
  return cgl_scale3(a, 1.0f / len);
}

static inline cgl_vec3 cgl_lerp3(cgl_vec3 a, cgl_vec3 b, float t) {
  return cgl_add3(a, cgl_scale3(cgl_sub3(b, a), t));
}

static inline cgl_vec3 cgl_reflect3(cgl_vec3 i, cgl_vec3 n) {
  return cgl_sub3(i, cgl_scale3(n, 2.0f * cgl_dot3(i, n)));
}

/* n must be unit. Returns false on total internal reflection. */
static inline bool cgl_refract3(cgl_vec3 i, cgl_vec3 n, float eta, cgl_vec3 *out) {
  float cosi = -cgl_dot3(i, n);
  float sin2t = eta * eta * (1.0f - cosi * cosi);
  if (sin2t > 1.0f) {
    return false;
  }
  float cost = sqrtf(1.0f - sin2t);
  *out = cgl_add3(cgl_scale3(i, eta), cgl_scale3(n, eta * cosi - cost));
  return true;
}

static inline cgl_vec3 cgl_min3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z));
}

static inline cgl_vec3 cgl_max3(cgl_vec3 a, cgl_vec3 b) {
  return cgl_v3(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z));
}

static inline cgl_vec3 cgl_clamp3(cgl_vec3 a, float lo, float hi) {
  return cgl_v3(cgl_clampf(a.x, lo, hi), cgl_clampf(a.y, lo, hi),
                cgl_clampf(a.z, lo, hi));
}

static inline cgl_mat4 cgl_mat4_identity(void) {
  cgl_mat4 r;
  for (int i = 0; i < 16; ++i) {
    r.m[i] = 0.0f;
  }
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
  return r;
}

static inline cgl_vec4 cgl_mat4_mulv(const cgl_mat4 *a, cgl_vec4 v) {
  cgl_vec4 o;
  o.x = a->m[0] * v.x + a->m[4] * v.y + a->m[8] * v.z + a->m[12] * v.w;
  o.y = a->m[1] * v.x + a->m[5] * v.y + a->m[9] * v.z + a->m[13] * v.w;
  o.z = a->m[2] * v.x + a->m[6] * v.y + a->m[10] * v.z + a->m[14] * v.w;
  o.w = a->m[3] * v.x + a->m[7] * v.y + a->m[11] * v.z + a->m[15] * v.w;
  return o;
}

static inline cgl_mat4 cgl_mat4_mul(const cgl_mat4 *a, const cgl_mat4 *b) {
  cgl_mat4 r;
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) {
        s += a->m[k * 4 + row] * b->m[c * 4 + k];
      }
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

static inline cgl_vec3 cgl_mat4_mul_point(const cgl_mat4 *a, cgl_vec3 p) {
  cgl_vec4 v = cgl_mat4_mulv(a, cgl_v4(p.x, p.y, p.z, 1.0f));
  if (fabsf(v.w) > 1e-20f) {
    return cgl_v3(v.x / v.w, v.y / v.w, v.z / v.w);
  }
  return cgl_v3(v.x, v.y, v.z);
}

static inline cgl_vec3 cgl_mat4_mul_dir(const cgl_mat4 *a, cgl_vec3 d) {
  cgl_vec4 v = cgl_mat4_mulv(a, cgl_v4(d.x, d.y, d.z, 0.0f));
  return cgl_v3(v.x, v.y, v.z);
}

static inline cgl_mat4 cgl_translate(cgl_vec3 t) {
  cgl_mat4 r = cgl_mat4_identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

static inline cgl_mat4 cgl_scale(cgl_vec3 s) {
  cgl_mat4 r = cgl_mat4_identity();
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  return r;
}

static inline cgl_mat4 cgl_rotate_x(float rad) {
  cgl_mat4 r = cgl_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  return r;
}

static inline cgl_mat4 cgl_rotate_y(float rad) {
  cgl_mat4 r = cgl_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

static inline cgl_mat4 cgl_rotate_z(float rad) {
  cgl_mat4 r = cgl_mat4_identity();
  float c = cosf(rad), s = sinf(rad);
  r.m[0] = c;
  r.m[1] = s;
  r.m[4] = -s;
  r.m[5] = c;
  return r;
}

/* Invert a general 4x4 (Gauss-Jordan). Returns false if singular. */
static inline bool cgl_mat4_invert(const cgl_mat4 *m, cgl_mat4 *out) {
  float a[4][8];
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      a[r][c] = m->m[c * 4 + r];
      a[r][c + 4] = (r == c) ? 1.0f : 0.0f;
    }
  }
  for (int col = 0; col < 4; ++col) {
    int pivot = col;
    for (int r = col; r < 4; ++r) {
      if (fabsf(a[r][col]) > fabsf(a[pivot][col])) {
        pivot = r;
      }
    }
    if (fabsf(a[pivot][col]) < 1e-8f) {
      return false;
    }
    if (pivot != col) {
      for (int k = 0; k < 8; ++k) {
        float tmp = a[col][k];
        a[col][k] = a[pivot][k];
        a[pivot][k] = tmp;
      }
    }
    float inv = 1.0f / a[col][col];
    for (int k = 0; k < 8; ++k) {
      a[col][k] *= inv;
    }
    for (int r = 0; r < 4; ++r) {
      if (r == col) {
        continue;
      }
      float f = a[r][col];
      for (int k = 0; k < 8; ++k) {
        a[r][k] -= f * a[col][k];
      }
    }
  }
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      out->m[c * 4 + r] = a[r][c + 4];
    }
  }
  return true;
}

/* Right-handed look-at, camera at eye looking at center, y-up. */
static inline cgl_mat4 cgl_look_at(cgl_vec3 eye, cgl_vec3 center, cgl_vec3 up) {
  cgl_vec3 f = cgl_normalize3(cgl_sub3(center, eye));
  cgl_vec3 s = cgl_normalize3(cgl_cross3(f, up));
  cgl_vec3 u = cgl_cross3(s, f);
  cgl_mat4 r = cgl_mat4_identity();
  r.m[0] = s.x;
  r.m[4] = s.y;
  r.m[8] = s.z;
  r.m[1] = u.x;
  r.m[5] = u.y;
  r.m[9] = u.z;
  r.m[2] = -f.x;
  r.m[6] = -f.y;
  r.m[10] = -f.z;
  r.m[12] = -cgl_dot3(s, eye);
  r.m[13] = -cgl_dot3(u, eye);
  r.m[14] = cgl_dot3(f, eye);
  return r;
}

/* Right-handed perspective, OpenGL-style NDC z in [-1,1]. */
static inline cgl_mat4 cgl_perspective(float fovy_rad, float aspect, float znear,
                                       float zfar) {
  float t = tanf(fovy_rad * 0.5f);
  cgl_mat4 r;
  for (int i = 0; i < 16; ++i) {
    r.m[i] = 0.0f;
  }
  r.m[0] = 1.0f / (aspect * t);
  r.m[5] = 1.0f / t;
  r.m[10] = -(zfar + znear) / (zfar - znear);
  r.m[11] = -1.0f;
  r.m[14] = -(2.0f * zfar * znear) / (zfar - znear);
  return r;
}

static inline cgl_mat4 cgl_ortho(float l, float r, float b, float t, float n,
                                 float f) {
  cgl_mat4 m = cgl_mat4_identity();
  m.m[0] = 2.0f / (r - l);
  m.m[5] = 2.0f / (t - b);
  m.m[10] = -2.0f / (f - n);
  m.m[12] = -(r + l) / (r - l);
  m.m[13] = -(t + b) / (t - b);
  m.m[14] = -(f + n) / (f - n);
  return m;
}

#endif /* CGL_VEC_H */
