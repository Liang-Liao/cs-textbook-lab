/* Demo: computational geometry (CLRS Ch.33). */
#include <stdio.h>

#include "geometry.h"

int main(void) {
  printf("=== CLRS 33.1 Segment intersection ===\n");
  Point a = {0, 0}, b = {1, 1}, c = {0, 1}, d = {1, 0};
  printf("(0,0)-(1,1) x (0,1)-(1,0): %s\n",
         segments_intersect(&a, &b, &c, &d) ? "yes" : "no");
  Point e = {0, 0}, f = {1, 0}, g = {0, 1}, h = {1, 1};
  printf("(0,0)-(1,0) x (0,1)-(1,1): %s\n",
         segments_intersect(&e, &f, &g, &h) ? "yes" : "no");

  printf("\n=== Convex hull (Jarvis march) ===\n");
  Point pts[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0.5}};
  Point hull[8];
  size_t n = convex_hull_jarvis(pts, 5, hull, 8);
  printf("hull (%zu):", n);
  for (size_t i = 0; i < n; i++) {
    printf(" (%g,%g)", hull[i].x, hull[i].y);
  }
  printf("\n");
  return 0;
}
