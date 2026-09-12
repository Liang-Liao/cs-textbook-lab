#ifndef CLRS_GEOMETRY_H
#define CLRS_GEOMETRY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS Ch.33 Computational geometry — 2D points.
 */

typedef struct {
  double x, y;
} Point;

/* CLRS 33.1 cross product orientation of ordered triple (p,q,r).
 * >0 left turn (counterclockwise), <0 right turn, 0 collinear. */
double cross(const Point *p, const Point *q, const Point *r);

/* CLRS 33.1 ON-SEGMENT(p,q,r): q collinear and on segment pr. */
int on_segment(const Point *p, const Point *q, const Point *r);

/* CLRS 33.1 SEGMENTS-INTERSECT(p1,p2,p3,p4). */
int segments_intersect(const Point *p1, const Point *p2, const Point *p3,
                       const Point *p4);

/*
 * Convex hull (Jarvis march / gift wrapping).
 * points[0..n-1], n >= 1. Writes hull vertices in counterclockwise order
 * into hull_out (max maxn). Returns hull size; size maxn >= n always
 * suffices (a hull never has more vertices than distinct input points).
 * Duplicate points (exact coordinate equality): first occurrence wins.
 */
size_t convex_hull_jarvis(const Point *points, size_t n, Point *hull_out,
                          size_t maxn);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_GEOMETRY_H */
