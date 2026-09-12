#include "geometry.h"

#include <math.h>

#include "clrs.h"

double cross(const Point *p, const Point *q, const Point *r) {
  return (q->x - p->x) * (r->y - p->y) - (q->y - p->y) * (r->x - p->x);
}

int on_segment(const Point *p, const Point *q, const Point *r) {
  if (fabs(cross(p, q, r)) > 1e-12) {
    return 0;
  }
  return (q->x >= (p->x < r->x ? p->x : r->x) - 1e-12) &&
         (q->x <= (p->x > r->x ? p->x : r->x) + 1e-12) &&
         (q->y >= (p->y < r->y ? p->y : r->y) - 1e-12) &&
         (q->y <= (p->y > r->y ? p->y : r->y) + 1e-12);
}

/* CLRS 33.1 SEGMENTS-INTERSECT */
int segments_intersect(const Point *p1, const Point *p2, const Point *p3,
                       const Point *p4) {
  double d1 = cross(p3, p4, p1);
  double d2 = cross(p3, p4, p2);
  double d3 = cross(p1, p2, p3);
  double d4 = cross(p1, p2, p4);

  const double eps = 1e-12;
  if (((d1 > eps && d2 < -eps) || (d1 < -eps && d2 > eps)) &&
      ((d3 > eps && d4 < -eps) || (d3 < -eps && d4 > eps))) {
    return 1;
  }
  if (fabs(d1) <= eps && on_segment(p3, p1, p4)) {
    return 1;
  }
  if (fabs(d2) <= eps && on_segment(p3, p2, p4)) {
    return 1;
  }
  if (fabs(d3) <= eps && on_segment(p1, p3, p2)) {
    return 1;
  }
  if (fabs(d4) <= eps && on_segment(p1, p4, p2)) {
    return 1;
  }
  return 0;
}

size_t convex_hull_jarvis(const Point *points, size_t n, Point *hull_out,
                          size_t maxn) {
  if (n == 0 || maxn == 0) {
    return 0;
  }

  /* deduplicate (exact coordinate equality), first occurrence wins, so
   * repeated points cannot end up on the hull */
  Point *pts = clrs_xmalloc(n * sizeof(Point));
  size_t m = 0;
  for (size_t i = 0; i < n; i++) {
    int dup = 0;
    for (size_t j = 0; j < m; j++) {
      if (pts[j].x == points[i].x && pts[j].y == points[i].y) {
        dup = 1;
        break;
      }
    }
    if (!dup) {
      pts[m++] = points[i];
    }
  }
  if (m == 1) {
    hull_out[0] = pts[0];
    free(pts);
    return 1;
  }

  /* leftmost lowest point as start */
  size_t start = 0;
  for (size_t i = 1; i < m; i++) {
    if (pts[i].x < pts[start].x ||
        (pts[i].x == pts[start].x && pts[i].y < pts[start].y)) {
      start = i;
    }
  }

  size_t hull_n = 0;
  size_t p = start;
  do {
    if (hull_n >= maxn) {
      break;
    }
    hull_out[hull_n++] = pts[p];

    size_t q = (p + 1) % m;
    for (size_t i = 0; i < m; i++) {
      double c = cross(&pts[p], &pts[q], &pts[i]);
      if (c > 1e-12 || (fabs(c) <= 1e-12 &&
                        (pts[i].x - pts[p].x) * (pts[i].x - pts[p].x) +
                                (pts[i].y - pts[p].y) * (pts[i].y - pts[p].y) >
                            (pts[q].x - pts[p].x) * (pts[q].x - pts[p].x) +
                                (pts[q].y - pts[p].y) *
                                    (pts[q].y - pts[p].y))) {
        q = i;
      }
    }
    p = q;
  } while (p != start && hull_n < maxn);

  free(pts);
  return hull_n;
}
