#ifndef CGL_ASSERT_H
#define CGL_ASSERT_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CGL_CHECK(cond, msg)                                                   \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "CHECK failed: %s (%s:%d)\n", (msg), __FILE__,           \
              __LINE__);                                                       \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define CGL_CHECK_NEAR(a, b, eps, msg)                                         \
  CGL_CHECK(fabs((double)(a) - (double)(b)) <= (eps), msg)

#endif /* CGL_ASSERT_H */
