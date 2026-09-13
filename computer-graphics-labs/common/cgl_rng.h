#ifndef CGL_RNG_H
#define CGL_RNG_H

#include <stdint.h>

typedef struct {
  uint64_t s;
} cgl_rng;

static inline void cgl_rng_seed(cgl_rng *r, uint64_t seed) {
  r->s = seed ? seed : 0x9e3779b97f4a7c15ull;
}

static inline uint64_t cgl_rng_u64(cgl_rng *r) {
  /* splitmix64 */
  uint64_t z = (r->s += 0x9e3779b97f4a7c15ull);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

static inline float cgl_rng_next01(cgl_rng *r) {
  return (float)((cgl_rng_u64(r) >> 40) * (1.0 / 16777216.0));
}

#endif /* CGL_RNG_H */
