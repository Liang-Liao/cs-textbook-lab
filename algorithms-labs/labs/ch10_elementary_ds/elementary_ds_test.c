#include <stdio.h>
#include <stdlib.h>

#include "binary_tree.h"
#include "clrs.h"
#include "linked_list.h"
#include "queue.h"
#include "stack.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* --- stack --- */
  {
    IntStack s;
    stack_init(&s, 8);
    ASSERT_TRUE(&t, stack_empty(&s));
    stack_push(&s, 1);
    stack_push(&s, 2);
    stack_push(&s, 3);
    ASSERT_TRUE(&t, stack_full(&s) == 0);
    ASSERT_EQ_INT(&t, stack_peek(&s), 3);
    ASSERT_EQ_INT(&t, stack_pop(&s), 3);
    ASSERT_EQ_INT(&t, stack_pop(&s), 2);
    ASSERT_EQ_INT(&t, stack_pop(&s), 1);
    ASSERT_TRUE(&t, stack_empty(&s));
    stack_push(&s, 9);
    ASSERT_EQ_INT(&t, stack_pop(&s), 9);
    stack_destroy(&s);
  }

  /* --- queue --- */
  {
    IntQueue q;
    queue_init(&q, 5); /* max 4 elements */
    ASSERT_TRUE(&t, queue_empty(&q));
    queue_enqueue(&q, 1);
    queue_enqueue(&q, 2);
    queue_enqueue(&q, 3);
    queue_enqueue(&q, 4);
    ASSERT_TRUE(&t, queue_full(&q));
    ASSERT_EQ_INT(&t, queue_head(&q), 1);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 1);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 2);
    queue_enqueue(&q, 5);
    queue_enqueue(&q, 6);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 3);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 4);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 5);
    ASSERT_EQ_INT(&t, queue_dequeue(&q), 6);
    ASSERT_TRUE(&t, queue_empty(&q));
    queue_destroy(&q);
  }

  /* --- doubly linked list --- */
  {
    DoublyList L;
    dlist_init(&L);
    ASSERT_TRUE(&t, dlist_length(&L) == 0);
    dlist_append(&L, 10);
    dlist_append(&L, 20);
    dlist_prepend(&L, 5);
    ASSERT_EQ_INT(&t, (int)dlist_length(&L), 3);

    int buf[8];
    size_t n = dlist_to_array(&L, buf, 8);
    ASSERT_EQ_INT(&t, (int)n, 3);
    ASSERT_EQ_INT(&t, buf[0], 5);
    ASSERT_EQ_INT(&t, buf[1], 10);
    ASSERT_EQ_INT(&t, buf[2], 20);

    ListNode *x = dlist_search(&L, 10);
    ASSERT_TRUE(&t, x != NULL);
    dlist_delete(&L, x);
    ASSERT_EQ_INT(&t, (int)dlist_length(&L), 2);
    n = dlist_to_array(&L, buf, 8);
    ASSERT_EQ_INT(&t, buf[0], 5);
    ASSERT_EQ_INT(&t, buf[1], 20);

    ASSERT_TRUE(&t, dlist_search(&L, 99) == NULL);
    dlist_destroy(&L);
  }

  /* --- binary tree traversals ---
   *        4
   *       / \
   *      2   6
   *     / \ / \
   *    1  3 5  7
   * inorder 1,2,3,4,5,6,7
   * preorder 4,2,1,3,6,5,7
   * postorder 1,3,2,5,7,6,4
   */
  {
    TreeNode *r = tree_node_new(4);
    r->left = tree_node_new(2);
    r->right = tree_node_new(6);
    r->left->left = tree_node_new(1);
    r->left->right = tree_node_new(3);
    r->right->left = tree_node_new(5);
    r->right->right = tree_node_new(7);

    ASSERT_EQ_INT(&t, (int)tree_size(r), 7);
    ASSERT_EQ_INT(&t, (int)tree_height(r), 3); /* 3 levels, height as #nodes on path? */
    /* CLRS height of single node is 0. Our tree_height returns 1 + children:
     * single node: left=right=NULL → height 0+? we return 1+0=1 if both null...
     * Wait: if root only: tree_height returns 1 + max(0,0) = 1. That's "levels".
     * Document as number of nodes on longest path. Single node = 1.
     */

    int out[8];
    size_t n = tree_inorder(r, out, 8);
    ASSERT_EQ_INT(&t, (int)n, 7);
    int in_exp[] = {1, 2, 3, 4, 5, 6, 7};
    for (int i = 0; i < 7; i++) {
      ASSERT_EQ_INT(&t, out[i], in_exp[i]);
    }

    n = tree_preorder(r, out, 8);
    ASSERT_EQ_INT(&t, (int)n, 7);
    int pre_exp[] = {4, 2, 1, 3, 6, 5, 7};
    for (int i = 0; i < 7; i++) {
      ASSERT_EQ_INT(&t, out[i], pre_exp[i]);
    }

    n = tree_postorder(r, out, 8);
    ASSERT_EQ_INT(&t, (int)n, 7);
    int post_exp[] = {1, 3, 2, 5, 7, 6, 4};
    for (int i = 0; i < 7; i++) {
      ASSERT_EQ_INT(&t, out[i], post_exp[i]);
    }

    tree_free(r);
  }

  {
    TreeNode *leaf = tree_node_new(1);
    ASSERT_EQ_INT(&t, (int)tree_size(leaf), 1);
    ASSERT_EQ_INT(&t, (int)tree_height(leaf), 1);
    tree_free(leaf);
    ASSERT_EQ_INT(&t, (int)tree_size(NULL), 0);
  }

  return test_report(&t, "elementary_ds");
}
