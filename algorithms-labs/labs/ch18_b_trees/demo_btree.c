/* Demo: B-tree (CLRS Ch.18). */
#include <stdio.h>

#include "btree.h"

int main(void) {
  BTree bt;
  btree_init(&bt, 3);

  printf("=== CLRS 18.2 B-TREE-INSERT (t=3) ===\n");
  int keys[] = {10, 20, 5, 6, 12, 30, 7, 17};
  for (int i = 0; i < 8; i++) {
    btree_insert(&bt, keys[i]);
    printf("insert %2d  size=%zu height=%d\n", keys[i], btree_size(&bt),
           btree_height(&bt));
  }

  int out[16];
  size_t n = btree_inorder(&bt, out, 16);
  printf("inorder:");
  for (size_t i = 0; i < n; i++) {
    printf(" %d", out[i]);
  }
  printf("\n");

  printf("\n=== B-TREE-DELETE ===\n");
  int dels[] = {6, 13, 7, 4, 20};
  for (int i = 0; i < 5; i++) {
    int ok = btree_delete(&bt, dels[i]);
    printf("delete %d: %s  size=%zu\n", dels[i], ok ? "ok" : "absent",
           btree_size(&bt));
  }
  n = btree_inorder(&bt, out, 16);
  printf("inorder:");
  for (size_t i = 0; i < n; i++) {
    printf(" %d", out[i]);
  }
  printf("\n");

  btree_destroy(&bt);
  return 0;
}
