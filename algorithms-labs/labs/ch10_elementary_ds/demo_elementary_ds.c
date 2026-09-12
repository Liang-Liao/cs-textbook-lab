/* Demo: stack, queue, list, tree (CLRS Ch.10). */
#include <stdio.h>

#include "binary_tree.h"
#include "linked_list.h"
#include "queue.h"
#include "stack.h"

static void print_arr(const char *label, const int *a, size_t n) {
  printf("%s:", label);
  for (size_t i = 0; i < n; i++) {
    printf(" %d", a[i]);
  }
  printf("\n");
}

int main(void) {
  printf("=== CLRS 10.1 Stack ===\n");
  IntStack s;
  stack_init(&s, 8);
  for (int i = 1; i <= 4; i++) {
    stack_push(&s, i);
  }
  printf("push 1..4, pop order:");
  while (!stack_empty(&s)) {
    printf(" %d", stack_pop(&s));
  }
  printf("\n");
  stack_destroy(&s);

  printf("\n=== CLRS 10.1 Queue ===\n");
  IntQueue q;
  queue_init(&q, 8);
  for (int i = 10; i <= 13; i++) {
    queue_enqueue(&q, i);
  }
  printf("enqueue 10..13, dequeue order:");
  while (!queue_empty(&q)) {
    printf(" %d", queue_dequeue(&q));
  }
  printf("\n");
  queue_destroy(&q);

  printf("\n=== CLRS 10.2 Doubly linked list ===\n");
  DoublyList L;
  dlist_init(&L);
  dlist_append(&L, 10);
  dlist_append(&L, 20);
  dlist_append(&L, 30);
  dlist_prepend(&L, 5);
  int buf[8];
  size_t n = dlist_to_array(&L, buf, 8);
  print_arr("list", buf, n);
  ListNode *x = dlist_search(&L, 20);
  if (x) {
    dlist_delete(&L, x);
  }
  n = dlist_to_array(&L, buf, 8);
  print_arr("after delete 20", buf, n);
  dlist_destroy(&L);

  printf("\n=== CLRS 10.4 Binary tree traversals ===\n");
  TreeNode *r = tree_node_new(4);
  r->left = tree_node_new(2);
  r->right = tree_node_new(6);
  r->left->left = tree_node_new(1);
  r->left->right = tree_node_new(3);
  r->right->left = tree_node_new(5);
  r->right->right = tree_node_new(7);

  int out[8];
  n = tree_inorder(r, out, 8);
  print_arr("inorder  ", out, n);
  n = tree_preorder(r, out, 8);
  print_arr("preorder ", out, n);
  n = tree_postorder(r, out, 8);
  print_arr("postorder", out, n);
  tree_free(r);

  return 0;
}
