/* Demo: red-black tree (CLRS Ch.13). */
#include <stdio.h>

#include "red_black_tree.h"

int main(void) {
  RBTree tr;
  rb_init(&tr);

  printf("=== CLRS 13.3 RB-INSERT ===\n");
  int keys[] = {41, 38, 31, 12, 19, 8};
  for (int i = 0; i < 6; i++) {
    rb_insert(&tr, keys[i]);
    printf("insert %d  valid=%d\n", keys[i], rb_validate(&tr));
  }

  int out[16];
  size_t n = rb_inorder(&tr, out, 16);
  printf("inorder:");
  for (size_t i = 0; i < n; i++) {
    printf(" %d", out[i]);
  }
  printf("\n");

  printf("\n=== CLRS 13.4 RB-DELETE ===\n");
  int dels[] = {8, 12, 19};
  for (int i = 0; i < 3; i++) {
    rb_delete(&tr, dels[i]);
    printf("delete %d  valid=%d\n", dels[i], rb_validate(&tr));
  }
  n = rb_inorder(&tr, out, 16);
  printf("inorder:");
  for (size_t i = 0; i < n; i++) {
    printf(" %d", out[i]);
  }
  printf("\n");

  rb_destroy(&tr);
  return 0;
}
