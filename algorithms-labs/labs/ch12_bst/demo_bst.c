/* Demo: binary search tree (CLRS Ch.12). */
#include <stdio.h>

#include "bst.h"

static void print_inorder(const BST *b) {
  int out[32];
  size_t n = bst_inorder(b, out, 32);
  for (size_t i = 0; i < n; i++) {
    printf("%d ", out[i]);
  }
  printf("\n");
}

int main(void) {
  BST b;
  bst_init(&b);

  int keys[] = {15, 6, 18, 3, 7, 17, 20, 2, 4, 13, 9};
  printf("=== CLRS 12.3 TREE-INSERT ===\ninsert:");
  for (int i = 0; i < 11; i++) {
    printf(" %d", keys[i]);
    bst_insert(&b, keys[i]);
  }
  printf("\ninorder: ");
  print_inorder(&b);
  printf("min=%d max=%d\n", bst_minimum(&b)->key, bst_maximum(&b)->key);

  BSTNode *x = bst_search(&b, 15);
  printf("successor(15)=%d predecessor(15)=%d\n",
         bst_successor(x)->key, bst_predecessor(x)->key);

  printf("\n=== CLRS 12.3 TREE-DELETE ===\n");
  printf("delete 6 (two children)\n");
  bst_delete(&b, 6);
  printf("inorder: ");
  print_inorder(&b);
  printf("delete 15 (root, two children)\n");
  bst_delete(&b, 15);
  printf("inorder: ");
  print_inorder(&b);

  bst_destroy(&b);
  return 0;
}
