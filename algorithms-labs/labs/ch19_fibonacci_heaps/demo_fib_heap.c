/* Demo: Fibonacci heap (CLRS Ch.19). */
#include <stdio.h>

#include "fib_heap.h"

int main(void) {
  FibHeap h;
  fib_init(&h);

  printf("=== CLRS 19.1 FIB-HEAP-INSERT / EXTRACT-MIN ===\n");
  int keys[] = {7, 3, 17, 24, 10, 1, 5};
  for (int i = 0; i < 7; i++) {
    fib_insert(&h, keys[i]);
  }
  printf("inserted 7 3 17 24 10 1 5; min=%d\n", fib_minimum(&h));

  printf("extract order:");
  while (fib_size(&h) > 0) {
    printf(" %d", fib_extract_min(&h));
  }
  printf("\n");

  printf("\n=== DECREASE-KEY ===\n");
  fib_init(&h);
  fib_insert(&h, 10);
  fib_insert(&h, 15);
  FibNode *c = fib_insert(&h, 20);
  printf("min=%d; decrease %d -> 3\n", fib_minimum(&h), c->key);
  fib_decrease_key(&h, c, 3);
  printf("min=%d\n", fib_minimum(&h));
  fib_destroy(&h);

  return 0;
}
