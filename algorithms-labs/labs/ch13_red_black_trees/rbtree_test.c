#include <stdio.h>
#include <stdlib.h>

#include "array.h"
#include "clrs.h"
#include "red_black_tree.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* empty */
  {
    RBTree tr;
    rb_init(&tr);
    ASSERT_TRUE(&t, rb_search(&tr, 1) == NULL);
    ASSERT_TRUE(&t, rb_minimum(&tr) == NULL);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_destroy(&tr);
  }

  /* sequential inserts — tree must stay a valid RB tree */
  {
    RBTree tr;
    rb_init(&tr);
    for (int i = 1; i <= 31; i++) {
      ASSERT_TRUE(&t, rb_insert(&tr, i) != NULL);
      ASSERT_TRUE(&t, rb_validate(&tr));
    }
    ASSERT_EQ_INT(&t, (int)rb_size(&tr), 31);
    ASSERT_EQ_INT(&t, rb_minimum(&tr)->key, 1);
    ASSERT_EQ_INT(&t, rb_maximum(&tr)->key, 31);

    int out[32];
    size_t n = rb_inorder(&tr, out, 32);
    ASSERT_EQ_INT(&t, (int)n, 31);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));

    /* duplicate */
    ASSERT_TRUE(&t, rb_insert(&tr, 10) == NULL);

    /* successor chain */
    RBNode *x = rb_search(&tr, 1);
    for (int i = 2; i <= 31; i++) {
      x = rb_successor(&tr, x);
      ASSERT_EQ_INT(&t, x->key, i);
    }
    ASSERT_TRUE(&t, rb_successor(&tr, x) == NULL);

    /* delete every key, keep validating */
    for (int i = 1; i <= 31; i++) {
      ASSERT_TRUE(&t, rb_delete(&tr, i));
      ASSERT_TRUE(&t, rb_validate(&tr));
    }
    ASSERT_EQ_INT(&t, (int)rb_size(&tr), 0);
    ASSERT_TRUE(&t, !rb_delete(&tr, 1));
    rb_destroy(&tr);
  }

  /* reverse order */
  {
    RBTree tr;
    rb_init(&tr);
    for (int i = 100; i >= 1; i--) {
      rb_insert(&tr, i);
      ASSERT_TRUE(&t, rb_validate(&tr));
    }
    for (int i = 1; i <= 100; i++) {
      rb_delete(&tr, i);
      ASSERT_TRUE(&t, rb_validate(&tr));
    }
    rb_destroy(&tr);
  }

  /* book-style figure keys mixed */
  {
    RBTree tr;
    rb_init(&tr);
    int keys[] = {41, 38, 31, 12, 19, 8};
    for (int i = 0; i < 6; i++) {
      rb_insert(&tr, keys[i]);
      ASSERT_TRUE(&t, rb_validate(&tr));
    }
    rb_delete(&tr, 8);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_delete(&tr, 12);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_delete(&tr, 19);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_delete(&tr, 31);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_delete(&tr, 38);
    ASSERT_TRUE(&t, rb_validate(&tr));
    rb_delete(&tr, 41);
    ASSERT_EQ_INT(&t, (int)rb_size(&tr), 0);
    rb_destroy(&tr);
  }

  /* random insert/delete stress */
  {
    RBTree tr;
    rb_init(&tr);
    int keys[80];
    array_fill_random(keys, 80, 99u, 0, 300);
    for (int i = 0; i < 80; i++) {
      rb_insert(&tr, keys[i]);
    }
    ASSERT_TRUE(&t, rb_validate(&tr));
    for (int i = 0; i < 80; i += 3) {
      rb_delete(&tr, keys[i]);
    }
    ASSERT_TRUE(&t, rb_validate(&tr));

    int out[128];
    size_t n = rb_inorder(&tr, out, 128);
    ASSERT_TRUE(&t, array_is_sorted_int(out, n));
    ASSERT_EQ_INT(&t, (int)n, (int)rb_size(&tr));
    rb_destroy(&tr);
  }

  return test_report(&t, "red_black_tree");
}
