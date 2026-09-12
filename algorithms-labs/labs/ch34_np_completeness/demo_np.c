/* Demo: NP-complete decision problems (CLRS Ch.34). */
#include <stdio.h>

#include "np_complete.h"

int main(void) {
  printf("=== CLRS 34 Decision problems ===\n");
  int eu[] = {0, 1, 2};
  int ev[] = {1, 2, 0};
  printf("Triangle K3: VC<=2? %d  IS>=2? %d  Clique>=3? %d\n",
         vertex_cover_decision(3, eu, ev, 3, 2),
         independent_set_decision(3, eu, ev, 3, 2),
         clique_decision(3, eu, ev, 3, 3));

  int clauses[] = {1, 2, 3, -1, -2, -3};
  printf("3-SAT (x1∨x2∨x3)∧(¬x1∨¬x2∨¬x3): %d\n",
         sat3_decision(3, clauses, 2));
  return 0;
}
