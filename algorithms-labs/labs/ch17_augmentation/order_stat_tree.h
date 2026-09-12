#ifndef CLRS_ORDER_STAT_TREE_H
#define CLRS_ORDER_STAT_TREE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 14.2 Order-statistic tree — red-black tree with subtree size.
 * Guarantees O(lg n) insert/delete/select/rank.
 * 0-based rank: select(0) = minimum.
 */

typedef enum { OS_BLACK = 0, OS_RED = 1 } OSColor;

typedef struct OSTNode {
  int key;
  OSColor color;
  size_t size;
  struct OSTNode *left;
  struct OSTNode *right;
  struct OSTNode *parent;
} OSTNode;

typedef struct {
  OSTNode *nil;
  OSTNode *root;
} OSTree;

void ost_init(OSTree *t);
void ost_destroy(OSTree *t);
size_t ost_size(const OSTree *t);

OSTNode *ost_insert(OSTree *t, int key);
int ost_delete(OSTree *t, int key);

/* OS-SELECT(i): (i+1)-th smallest, i in [0, n). */
OSTNode *ost_select(const OSTree *t, size_t i);

/* OS-RANK: 0-based order of key; n if absent. */
size_t ost_rank(const OSTree *t, int key);

OSTNode *ost_search(const OSTree *t, int key);

/* Validate RB + size fields; 1 if ok. */
int ost_validate(const OSTree *t);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_ORDER_STAT_TREE_H */
