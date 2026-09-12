#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "bst.h"
#include "clrs.h"
#include "test.h"

static int is_sorted_equal(const int *got, const int *exp, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (got[i] != exp[i]) {
      return 0;
    }
  }
  return 1;
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* empty */
  {
    BST b;
    bst_init(&b);
    ASSERT_TRUE(&t, bst_search(&b, 1) == NULL);
    ASSERT_TRUE(&t, bst_minimum(&b) == NULL);
    ASSERT_EQ_INT(&t, (int)bst_size(&b), 0);
    bst_destroy(&b);
  }

  /* Book-style insert sequence: 15, 6, 18, 3, 7, 17, 20, 2, 4, 13, 9 */
  {
    BST b;
    bst_init(&b);
    int keys[] = {15, 6, 18, 3, 7, 17, 20, 2, 4, 13, 9};
    for (size_t i = 0; i < 11; i++) {
      ASSERT_TRUE(&t, bst_insert(&b, keys[i]) != NULL);
    }
    ASSERT_EQ_INT(&t, (int)bst_size(&b), 11);
    ASSERT_EQ_INT(&t, bst_minimum(&b)->key, 2);
    ASSERT_EQ_INT(&t, bst_maximum(&b)->key, 20);

    int out[16];
    size_t n = bst_inorder(&b, out, 16);
    ASSERT_EQ_INT(&t, (int)n, 11);
    int sorted[] = {2, 3, 4, 6, 7, 9, 13, 15, 17, 18, 20};
    ASSERT_TRUE(&t, is_sorted_equal(out, sorted, 11));

    /* successor: 6 -> 7, 13 -> 15, 20 -> NULL */
    BSTNode *n6 = bst_search(&b, 6);
    ASSERT_EQ_INT(&t, bst_successor(n6)->key, 7);
    BSTNode *n13 = bst_search(&b, 13);
    ASSERT_EQ_INT(&t, bst_successor(n13)->key, 15);
    ASSERT_TRUE(&t, bst_successor(bst_search(&b, 20)) == NULL);

    /* predecessor: 15 -> 13, 2 -> NULL */
    ASSERT_EQ_INT(&t, bst_predecessor(bst_search(&b, 15))->key, 13);
    ASSERT_TRUE(&t, bst_predecessor(bst_search(&b, 2)) == NULL);

    /* duplicate insert ignored */
    ASSERT_TRUE(&t, bst_insert(&b, 7) == NULL);
    ASSERT_EQ_INT(&t, (int)bst_size(&b), 11);

    /* delete leaf: 2 */
    ASSERT_TRUE(&t, bst_delete(&b, 2));
    ASSERT_EQ_INT(&t, (int)bst_size(&b), 10);
    /* delete node with one child: 13 has only 9 */
    ASSERT_TRUE(&t, bst_delete(&b, 13));
    /* delete node with two children: 6 (children 3,7) */
    ASSERT_TRUE(&t, bst_delete(&b, 6));
    ASSERT_EQ_INT(&t, (int)bst_size(&b), 8);

    n = bst_inorder(&b, out, 16);
    int after[] = {3, 4, 7, 9, 15, 17, 18, 20};
    ASSERT_TRUE(&t, is_sorted_equal(out, after, 8));

    ASSERT_TRUE(&t, !bst_delete(&b, 99));
    bst_destroy(&b);
  }

  /* random: insert then delete half, inorder stays sorted */
  {
    BST b;
    bst_init(&b);
    int keys[50];
    array_fill_random(keys, 50, 42u, 0, 200);
    for (int i = 0; i < 50; i++) {
      bst_insert(&b, keys[i]); /* dups return NULL, ok */
    }
    size_t sz = bst_size(&b);
    int out[64];
    size_t n = bst_inorder(&b, out, 64);
    ASSERT_EQ_INT(&t, (int)n, (int)sz);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));

    for (int i = 0; i < 50; i += 2) {
      bst_delete(&b, keys[i]);
    }
    n = bst_inorder(&b, out, 64);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    ASSERT_TRUE(&t, (size_t)n == bst_size(&b));

    bst_destroy(&b);
  }

  return test_report(&t, "bst");
}
