#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "geometry.h"
#include "test.h"

static int almost(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* orientation */
  {
    Point p = {0, 0}, q = {1, 0}, r = {0, 1};
    ASSERT_TRUE(&t, cross(&p, &q, &r) > 0); /* left turn */
    r.y = -1;
    ASSERT_TRUE(&t, cross(&p, &q, &r) < 0); /* right */
    r.x = 2;
    r.y = 0;
    ASSERT_TRUE(&t, almost(cross(&p, &q, &r), 0, 1e-12)); /* collinear */
  }

  /* segments intersect — book-style crossing */
  {
    Point a = {0, 0}, b = {1, 1}, c = {0, 1}, d = {1, 0};
    ASSERT_TRUE(&t, segments_intersect(&a, &b, &c, &d));
  }
  {
    /* no intersection */
    Point a = {0, 0}, b = {1, 0}, c = {0, 1}, d = {1, 1};
    ASSERT_TRUE(&t, !segments_intersect(&a, &b, &c, &d));
  }
  {
    /* collinear overlap */
    Point a = {0, 0}, b = {2, 0}, c = {1, 0}, d = {3, 0};
    ASSERT_TRUE(&t, segments_intersect(&a, &b, &c, &d));
  }
  {
    /* endpoint touch */
    Point a = {0, 0}, b = {1, 0}, c = {1, 0}, d = {2, 1};
    ASSERT_TRUE(&t, segments_intersect(&a, &b, &c, &d));
  }
  {
    /* collinear no overlap */
    Point a = {0, 0}, b = {1, 0}, c = {2, 0}, d = {3, 0};
    ASSERT_TRUE(&t, !segments_intersect(&a, &b, &c, &d));
  }

  /* convex hull square */
  {
    Point pts[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0.5}};
    Point hull[8];
    size_t h = convex_hull_jarvis(pts, 5, hull, 8);
    ASSERT_EQ_INT(&t, (int)h, 4);
    /* all hull points among corners */
    for (size_t i = 0; i < h; i++) {
      int is_corner = (almost(hull[i].x, 0, 1e-9) || almost(hull[i].x, 1, 1e-9)) &&
                      (almost(hull[i].y, 0, 1e-9) || almost(hull[i].y, 1, 1e-9));
      ASSERT_TRUE(&t, is_corner);
    }
  }

  /* triangle */
  {
    Point pts[] = {{0, 0}, {1, 0}, {0.5, 1}};
    Point hull[4];
    size_t h = convex_hull_jarvis(pts, 3, hull, 4);
    ASSERT_EQ_INT(&t, (int)h, 3);
  }

  /* collinear points: hull is endpoints */
  {
    Point pts[] = {{0, 0}, {1, 0}, {2, 0}, {3, 0}};
    Point hull[8];
    size_t h = convex_hull_jarvis(pts, 4, hull, 8);
    ASSERT_TRUE(&t, h >= 2);
  }

  /* single point */
  {
    Point pts[] = {{5, 5}};
    Point hull[2];
    size_t h = convex_hull_jarvis(pts, 1, hull, 2);
    ASSERT_EQ_INT(&t, (int)h, 1);
    ASSERT_TRUE(&t, almost(hull[0].x, 5, 1e-9));
  }

  /* convex polygon all on hull */
  {
    Point pts[] = {{0, 0}, {2, 0}, {3, 1}, {2, 3}, {0, 3}, {-1, 1}};
    Point hull[8];
    size_t h = convex_hull_jarvis(pts, 6, hull, 8);
    ASSERT_EQ_INT(&t, (int)h, 6);
  }

  /* all-duplicate points: first occurrence wins, hull = 1 vertex */
  {
    Point pts[5] = {{2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}};
    Point hull[8];
    size_t h = convex_hull_jarvis(pts, 5, hull, 8);
    ASSERT_EQ_INT(&t, (int)h, 1);
    ASSERT_TRUE(&t, hull[0].x == 2 && hull[0].y == 2);
  }

  /* duplicates plus real corners */
  {
    Point pts[7] = {{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}, {4, 4}, {0, 4}};
    Point hull[8];
    size_t h = convex_hull_jarvis(pts, 7, hull, 8);
    ASSERT_EQ_INT(&t, (int)h, 4);
  }

  return test_report(&t, "geometry");
}
