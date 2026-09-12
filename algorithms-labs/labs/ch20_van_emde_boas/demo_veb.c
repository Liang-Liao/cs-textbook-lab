/* Demo: van Emde Boas tree (CLRS Ch.20). */
#include <stdio.h>

#include "veb.h"

int main(void) {
  VEB *v = veb_create(16);
  printf("=== CLRS 20 van Emde Boas (u=16) ===\n");
  int keys[] = {1, 3, 4, 7, 15, 8};
  for (int i = 0; i < 6; i++) {
    veb_insert(v, keys[i]);
  }
  printf("min=%d max=%d size=%zu\n", veb_minimum(v), veb_maximum(v),
         veb_size(v));
  printf("succ(4)=%d pred(8)=%d member(7)=%d\n", veb_successor(v, 4),
         veb_predecessor(v, 8), veb_member(v, 7));
  veb_delete(v, 4);
  printf("after delete 4: succ(3)=%d member(4)=%d\n", veb_successor(v, 3),
         veb_member(v, 4));
  veb_destroy(v);
  return 0;
}
