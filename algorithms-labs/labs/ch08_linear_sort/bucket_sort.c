#include "bucket_sort.h"

#include <stdlib.h>

#include "clrs.h"

typedef struct BNode {
  double key;
  struct BNode *next;
} BNode;

static void list_insert(BNode **head, double key) {
  BNode *node = clrs_xmalloc(sizeof(BNode));
  node->key = key;
  node->next = *head;
  *head = node;
}

/* Insertion sort on linked list of doubles (stable ascending). */
static BNode *list_insertion_sort(BNode *head) {
  BNode *sorted = NULL;
  while (head != NULL) {
    BNode *cur = head;
    head = head->next;
    if (sorted == NULL || cur->key < sorted->key) {
      cur->next = sorted;
      sorted = cur;
    } else {
      BNode *p = sorted;
      while (p->next != NULL && p->next->key <= cur->key) {
        p = p->next;
      }
      cur->next = p->next;
      p->next = cur;
    }
  }
  return sorted;
}

static void list_free(BNode *head) {
  while (head != NULL) {
    BNode *n = head->next;
    free(head);
    head = n;
  }
}

void bucket_sort_double(double *a, size_t n) {
  if (n < 2) {
    return;
  }

  BNode **buckets = clrs_xcalloc(n, sizeof(BNode *));

  for (size_t i = 0; i < n; i++) {
    CLRS_ASSERT(a[i] >= 0.0 && a[i] < 1.0, "bucket_sort expects [0,1)");
    size_t idx = (size_t)((double)n * a[i]);
    if (idx >= n) {
      idx = n - 1; /* guard 1.0-eps edge */
    }
    list_insert(&buckets[idx], a[i]);
  }

  size_t w = 0;
  for (size_t i = 0; i < n; i++) {
    buckets[i] = list_insertion_sort(buckets[i]);
    for (BNode *p = buckets[i]; p != NULL; p = p->next) {
      a[w++] = p->key;
    }
    list_free(buckets[i]);
  }
  free(buckets);
}
