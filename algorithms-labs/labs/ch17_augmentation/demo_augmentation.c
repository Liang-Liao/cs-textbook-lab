/* Demo: order-statistic and interval trees (CLRS Ch.14/17). */
#include <stdio.h>

#include "interval_tree.h"
#include "order_stat_tree.h"

int main(void) {
  printf("=== Order-statistic tree ===\n");
  OSTree st;
  ost_init(&st);
  int keys[] = {20, 10, 30, 5, 15, 25, 35};
  for (int i = 0; i < 7; i++) {
    ost_insert(&st, keys[i]);
  }
  printf("sorted by rank:");
  for (size_t i = 0; i < ost_size(&st); i++) {
    printf(" %d", ost_select(&st, i)->key);
  }
  printf("\nrank(25)=%zu  rank(5)=%zu\n", ost_rank(&st, 25), ost_rank(&st, 5));
  ost_destroy(&st);

  printf("\n=== Interval tree ===\n");
  ITree it;
  itree_init(&it);
  itree_insert(&it, 17, 19);
  itree_insert(&it, 5, 11);
  itree_insert(&it, 4, 8);
  itree_insert(&it, 15, 18);
  itree_insert(&it, 7, 10);
  itree_insert(&it, 16, 22);
  itree_insert(&it, 21, 23);

  int lo = 14, hi = 16;
  ITNode *x = itree_search_overlap(&it, lo, hi);
  if (x != NULL) {
    printf("query [%d,%d] overlaps [%d,%d]\n", lo, hi, x->iv.low, x->iv.high);
  }
  printf("count overlaps [%d,%d] = %zu\n", lo, hi,
         itree_count_overlaps(&it, lo, hi));
  itree_destroy(&it);
  return 0;
}
