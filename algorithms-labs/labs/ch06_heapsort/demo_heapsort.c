/* Demo: build heap, heapsort, priority queue (CLRS Ch.6). */
#include <stdio.h>

#include "array.h"
#include "heapsort.h"
#include "priority_queue.h"

static void print_arr(const char *label, const int *a, size_t n) {
  array_print_int(label, a, n);
}

int main(void) {
  int a[] = {4, 1, 3, 2, 16, 9, 10, 14, 8, 7};
  const size_t n = sizeof(a) / sizeof(a[0]);

  printf("=== CLRS 6.3 Build-Max-Heap ===\n");
  print_arr("input", a, n);
  build_max_heap(a, n);
  print_arr("heap ", a, n);
  printf("root (max) = %d\n", a[0]);

  printf("\n=== CLRS 6.4 Heapsort ===\n");
  int b[] = {5, 2, 4, 6, 1, 3};
  const size_t m = sizeof(b) / sizeof(b[0]);
  print_arr("before", b, m);
  heapsort_int(b, m);
  print_arr("after ", b, m);

  printf("\n=== CLRS 6.5 Max-priority queue ===\n");
  MaxPriorityQueue q;
  max_pq_init(&q, 16);
  int keys[] = {3, 1, 17, 5, 9, 12};
  for (size_t i = 0; i < 6; i++) {
    max_pq_insert(&q, keys[i]);
  }
  printf("inserted 3,1,17,5,9,12; maximum=%d\n", max_pq_maximum(&q));
  printf("extract-max order:");
  while (q.heap_size > 0) {
    printf(" %d", max_pq_extract_max(&q));
  }
  printf("\n");
  max_pq_destroy(&q);

  return 0;
}
