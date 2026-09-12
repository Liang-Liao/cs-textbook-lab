#include "linked_list.h"

#include <stdlib.h>

#include "clrs.h"

void dlist_init(DoublyList *L) {
  L->nil = clrs_xmalloc(sizeof(ListNode));
  L->nil->key = 0;
  L->nil->prev = L->nil;
  L->nil->next = L->nil;
}

void dlist_destroy(DoublyList *L) {
  ListNode *x = L->nil->next;
  while (x != L->nil) {
    ListNode *n = x->next;
    free(x);
    x = n;
  }
  free(L->nil);
  L->nil = NULL;
}

ListNode *dlist_search(DoublyList *L, int key) {
  ListNode *x = L->nil->next;
  while (x != L->nil && x->key != key) {
    x = x->next;
  }
  return (x == L->nil) ? NULL : x;
}

static void insert_after(DoublyList *L, ListNode *pos, int key) {
  CLRS_UNUSED(L);
  ListNode *x = clrs_xmalloc(sizeof(ListNode));
  x->key = key;
  x->prev = pos;
  x->next = pos->next;
  pos->next->prev = x;
  pos->next = x;
}

void dlist_prepend(DoublyList *L, int key) {
  insert_after(L, L->nil, key);
}

void dlist_append(DoublyList *L, int key) {
  insert_after(L, L->nil->prev, key);
}

void dlist_delete(DoublyList *L, ListNode *x) {
  CLRS_ASSERT(x != NULL && x != L->nil, "cannot delete sentinel/NULL");
  x->prev->next = x->next;
  x->next->prev = x->prev;
  free(x);
}

size_t dlist_length(const DoublyList *L) {
  size_t n = 0;
  for (ListNode *x = L->nil->next; x != L->nil; x = x->next) {
    n++;
  }
  return n;
}

size_t dlist_to_array(const DoublyList *L, int *out, size_t maxn) {
  size_t n = 0;
  for (ListNode *x = L->nil->next; x != L->nil && n < maxn; x = x->next) {
    out[n++] = x->key;
  }
  return n;
}
