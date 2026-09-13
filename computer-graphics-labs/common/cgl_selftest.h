#ifndef CGL_SELFTEST_H
#define CGL_SELFTEST_H

#include <math.h>
#include <stdio.h>

static int cgl_selftest_fails = 0;

#define CGL_EXPECT(cond, msg)                                                  \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "FAIL: %s\n", (msg));                                    \
      cgl_selftest_fails++;                                                    \
    }                                                                          \
  } while (0)

#define CGL_EXPECT_NEAR(a, b, eps, msg)                                        \
  do {                                                                         \
    double _a = (double)(a), _b = (double)(b);                                 \
    if (!(fabs(_a - _b) <= (eps))) {                                           \
      fprintf(stderr, "FAIL: %s (got %g, want %g)\n", (msg), _a, _b);          \
      cgl_selftest_fails++;                                                    \
    }                                                                          \
  } while (0)

static inline int cgl_selftest_report(const char *name) {
  if (cgl_selftest_fails == 0) {
    printf("SELF-TEST PASS: %s\n", name);
    return 0;
  }
  fprintf(stderr, "SELF-TEST FAIL: %s (%d failure(s))\n", name,
          cgl_selftest_fails);
  return 1;
}

#endif /* CGL_SELFTEST_H */
