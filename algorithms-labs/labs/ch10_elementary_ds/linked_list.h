#ifndef CLRS_LINKED_LIST_H
#define CLRS_LINKED_LIST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CLRS 10.2 Doubly linked list with sentinel.
 * Sentinel node is always present; list key values are ints.
 */
typedef struct ListNode {
  int key;
  struct ListNode *prev;
  struct ListNode *next;
} ListNode;

typedef struct {
  ListNode *nil; /* sentinel */
} DoublyList;

void dlist_init(DoublyList *L);
void dlist_destroy(DoublyList *L);

/* Search first node with key; returns NULL if absent. */
ListNode *dlist_search(DoublyList *L, int key);

/* Insert new node with key at the front (after sentinel). */
void dlist_prepend(DoublyList *L, int key);

/* Insert new node with key at the tail (before sentinel). */
void dlist_append(DoublyList *L, int key);

/* Delete node x (must belong to L). */
void dlist_delete(DoublyList *L, ListNode *x);

size_t dlist_length(const DoublyList *L);

/* Write keys front-to-back into out[0..maxn); returns count written. */
size_t dlist_to_array(const DoublyList *L, int *out, size_t maxn);

#ifdef __cplusplus
}
#endif

#endif /* CLRS_LINKED_LIST_H */
