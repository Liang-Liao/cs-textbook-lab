/* Demo: dynamic programming (CLRS Ch.14). */
#include <stdio.h>

#include "dp.h"

int main(void) {
  printf("=== CLRS 15.1 Rod cutting ===\n");
  int price[] = {1, 5, 8, 9, 10, 17, 17, 20, 24, 30};
  for (int n = 1; n <= 10; n++) {
    printf("n=%2d  bottom-up=%3lld  memo=%3lld\n", n,
           rod_cut_bottom_up(price, n), rod_cut_memo(price, n));
  }

  printf("\n=== CLRS 15.2 Matrix-chain ===\n");
  int dims[] = {30, 35, 15, 5, 10, 20, 25};
  printf("p = 30,35,15,5,10,20,25  min m[1,6] = %lld (book 15125)\n",
         matrix_chain_min(dims, 6, NULL));

  printf("\n=== CLRS 15.4 LCS ===\n");
  char lcs[32];
  size_t len = lcs_length("ABCBDAB", "BDCABA", lcs, 32);
  printf("X=ABCBDAB  Y=BDCABA\nLCS length=%zu  LCS=%s\n", len, lcs);

  return 0;
}
