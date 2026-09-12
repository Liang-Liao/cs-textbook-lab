/* Demo: disjoint-set forest (CLRS Ch.21). */
#include <stdio.h>

#include "disjoint_set.h"

int main(void) {
  printf("=== CLRS 21.3 Disjoint-set forest ===\n");
  DisjointSet s;
  ds_init(&s, 10);

  printf("union(0,1) union(2,3) union(1,2)\n");
  ds_union(&s, 0, 1);
  ds_union(&s, 2, 3);
  ds_union(&s, 1, 2);
  printf("connected(0,3)=%d  connected(0,4)=%d\n", ds_connected(&s, 0, 3),
         ds_connected(&s, 0, 4));

  ds_union(&s, 4, 5);
  ds_union(&s, 3, 5);
  printf("after union(4,5) union(3,5): connected(0,5)=%d\n",
         ds_connected(&s, 0, 5));

  printf("representatives:");
  for (size_t i = 0; i < 10; i++) {
    printf(" %d", ds_find(&s, i));
  }
  printf("\n");
  ds_destroy(&s);

  printf("\n=== Connected components ===\n");
  int eu[] = {0, 1, 3, 6};
  int ev[] = {1, 2, 4, 7};
  int comp[8];
  size_t k = ds_connected_components(8, eu, ev, 4, comp);
  printf("%zu components; comp[v] =", k);
  for (int i = 0; i < 8; i++) {
    printf(" %d", comp[i]);
  }
  printf("\n");
  return 0;
}
