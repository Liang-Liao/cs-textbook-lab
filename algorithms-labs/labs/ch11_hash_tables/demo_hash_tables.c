/* Demo: direct address, chaining, open addressing (CLRS Ch.11). */
#include <stdio.h>

#include "direct_address.h"
#include "hash_chain.h"
#include "hash_open_address.h"

int main(void) {
  printf("=== CLRS 11.1 Direct-address table ===\n");
  DirectAddress d;
  dad_init(&d, 10);
  dad_insert(&d, 2, 20);
  dad_insert(&d, 5, 50);
  printf("search(2)=%d search(5)=%d search(7)=%d\n", dad_search(&d, 2),
         dad_search(&d, 5), dad_search(&d, 7));
  dad_destroy(&d);

  printf("\n=== CLRS 11.2 Chaining ===\n");
  ChainedHash ht;
  chained_hash_init(&ht, 7);
  int keys[] = {10, 17, 3, 24, 5, 19};
  for (int i = 0; i < 6; i++) {
    chained_hash_insert(&ht, keys[i], keys[i] * 10);
  }
  printf("inserted 10,17,3,24,5,19 into m=7\n");
  int v;
  if (chained_hash_search(&ht, 17, &v)) {
    printf("search(17)=%d\n", v);
  }
  if (chained_hash_search(&ht, 99, &v)) {
    printf("search(99)=%d\n", v);
  } else {
    printf("search(99)=absent\n");
  }
  chained_hash_destroy(&ht);

  printf("\n=== CLRS 11.4 Open addressing (linear probe) ===\n");
  OpenHash oh;
  open_hash_init(&oh, 11);
  open_hash_insert(&oh, 10, 100);
  open_hash_insert(&oh, 21, 210); /* 10 mod 11 = 10, 21 mod 11 = 10 */
  printf("probe collision: 10 and 21 both hash to 10\n");
  printf("search(10)=%d search(21)=%d\n",
         (open_hash_search(&oh, 10, &v), v),
         (open_hash_search(&oh, 21, &v), v));
  open_hash_delete(&oh, 10);
  printf("after delete(10): search(21)=%d search(10)=%d\n",
         (open_hash_search(&oh, 21, &v), v),
         open_hash_search(&oh, 10, NULL));
  open_hash_destroy(&oh);

  return 0;
}
