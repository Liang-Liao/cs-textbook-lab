#ifndef CLRS_INTERVAL_TREE_H
#define CLRS_INTERVAL_TREE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 14.3 Interval trees — BST augmented with max high endpoint.
 * Intervals [low, high] inclusive; nodes keyed by low.
 */

typedef struct {
  int low;
  int high;
} Interval;

typedef struct ITNode {
  Interval iv;
  int max_high; /* max of high in this subtree */
  struct ITNode *left;
  struct ITNode *right;
  struct ITNode *parent;
} ITNode;

typedef struct {
  ITNode *root;
} ITree;

void itree_init(ITree *t);
void itree_destroy(ITree *t);

ITNode *itree_insert(ITree *t, int low, int high);
int itree_delete(ITree *t, int low);

/* CLRS INTERVAL-SEARCH: overlap with query [low,high]. NULL if none. */
ITNode *itree_search_overlap(const ITree *t, int low, int high);

/* Count overlaps among inserted intervals. */
size_t itree_count_overlaps(const ITree *t, int low, int high);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_INTERVAL_TREE_H */
