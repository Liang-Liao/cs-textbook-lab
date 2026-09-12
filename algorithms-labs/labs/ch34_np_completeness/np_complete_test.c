#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "np_complete.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Triangle: vertex cover size 2 */
  {
    int eu[] = {0, 1, 2};
    int ev[] = {1, 2, 0};
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 3, 1), 0);
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 3, 2), 1);
  }

  /* Path 0-1-2: VC size 1 (vertex 1) */
  {
    int eu[] = {0, 1};
    int ev[] = {1, 2};
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 2, 0), 0);
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 2, 1), 1);
  }

  /* Independent set: path of 3: max IS = 2 ({0,2}) */
  {
    int eu[] = {0, 1};
    int ev[] = {1, 2};
    ASSERT_EQ_INT(&t, independent_set_decision(3, eu, ev, 2, 2), 1);
    ASSERT_EQ_INT(&t, independent_set_decision(3, eu, ev, 2, 3), 0);
  }

  /* Clique: triangle is K3 */
  {
    int eu[] = {0, 1, 2};
    int ev[] = {1, 2, 0};
    ASSERT_EQ_INT(&t, clique_decision(3, eu, ev, 3, 3), 1);
    ASSERT_EQ_INT(&t, clique_decision(3, eu, ev, 3, 2), 1);
  }

  /* No clique of size 3 on a path */
  {
    int eu[] = {0, 1};
    int ev[] = {1, 2};
    ASSERT_EQ_INT(&t, clique_decision(3, eu, ev, 2, 3), 0);
  }

  /* 3-SAT: (x1 v x2 v x3) AND (~x1 v ~x2 v ~x3) is SAT */
  {
    int clauses[] = {
        1, 2, 3,    /* x1 x2 x3 */
        -1, -2, -3  /* ~x1 ~x2 ~x3 */
    };
    ASSERT_EQ_INT(&t, sat3_decision(3, clauses, 2), 1);
  }

  /* unsat: (x1) AND (~x1) encoded as 3-lits with repeats */
  {
    int clauses[] = {1, 1, 1, -1, -1, -1};
    ASSERT_EQ_INT(&t, sat3_decision(1, clauses, 2), 0);
  }

  /* (x1 v x2) AND (~x1) AND (~x2) unsat */
  {
    int clauses[] = {1, 2, 2, -1, -1, -1, -2, -2, -2};
    ASSERT_EQ_INT(&t, sat3_decision(2, clauses, 3), 0);
  }

  /* (x1 v ~x2) AND (x2) sat with x1=T,x2=T or x1=T,x2=F... x2=T then ~x2=F need x1 */
  {
    int clauses[] = {1, -2, -2, 2, 2, 2};
    ASSERT_EQ_INT(&t, sat3_decision(2, clauses, 2), 1);
  }

  /* Reduction demo: IS of size k in G iff VC of size n-k in G
     path of 3: n=3, IS>=2 <=> VC<=1 */
  {
    int eu[] = {0, 1};
    int ev[] = {1, 2};
    int is2 = independent_set_decision(3, eu, ev, 2, 2);
    int vc1 = vertex_cover_decision(3, eu, ev, 2, 1);
    ASSERT_EQ_INT(&t, is2, vc1);
  }

  /* boundary semantics: k < 0 */
  {
    int eu[] = {0, 1};
    int ev[] = {1, 2};
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 2, -1), 0);
    ASSERT_EQ_INT(&t, vertex_cover_decision(3, eu, ev, 0, -1), 0);
    ASSERT_EQ_INT(&t, independent_set_decision(3, eu, ev, 2, 0), 1);
    ASSERT_EQ_INT(&t, clique_decision(3, eu, ev, 2, 0), 1);
    ASSERT_EQ_INT(&t, clique_decision(3, eu, ev, 2, 1), 1);
    ASSERT_EQ_INT(&t, clique_decision(0, eu, ev, 0, 1), 0);
  }

  /* n > 20 returns -1 (unsupported) */
  {
    ASSERT_EQ_INT(&t, vertex_cover_decision(21, NULL, NULL, 0, 1), -1);
    ASSERT_EQ_INT(&t, independent_set_decision(21, NULL, NULL, 0, 1), -1);
    ASSERT_EQ_INT(&t, clique_decision(21, NULL, NULL, 0, 1), -1);
  }

  /* --- 3-SAT <=p CLIQUE reduction (CLRS 34.5, Theorem 34.10) --- */

  /* SAT formula from CLRS 34.5.1: (x1|~x2|~x3)(~x1|x2|x3)(x1|x2|x3) */
  {
    int clauses[] = {1, -2, -3, -1, 2, 3, 1, 2, 3};
    int eu[64], ev[64];
    size_t ne = sat3_to_clique(3, clauses, 3, eu, ev, 64);
    /* property check: edges only between clauses, never contradictory */
    for (size_t e = 0; e < ne; e++) {
      int c1 = eu[e] / 3, c2 = ev[e] / 3;
      ASSERT_TRUE(&t, c1 != c2);
      int l1 = clauses[c1 * 3 + eu[e] % 3];
      int l2 = clauses[c2 * 3 + ev[e] % 3];
      ASSERT_TRUE(&t, l1 != -l2);
    }
    /* completeness: every consistent cross-clause pair is an edge */
    size_t want = 0;
    for (int c1 = 0; c1 < 3; c1++) {
      for (int j1 = 0; j1 < 3; j1++) {
        for (int c2 = c1 + 1; c2 < 3; c2++) {
          for (int j2 = 0; j2 < 3; j2++) {
            if (clauses[c1 * 3 + j1] != -clauses[c2 * 3 + j2]) {
              want++;
            }
          }
        }
      }
    }
    ASSERT_EQ_INT(&t, (int)ne, (int)want);
    ASSERT_EQ_INT(&t, sat3_decision(3, clauses, 3), 1);
    ASSERT_EQ_INT(&t, clique_decision(9, eu, ev, ne, 3), 1);
  }

  /* UNSAT formula: reduction graph has no clique of size nclauses */
  {
    int clauses[] = {1, 1, 1, -1, -1, -1}; /* (x1)(~x1) */
    int eu[8], ev[8];
    size_t ne = sat3_to_clique(1, clauses, 2, eu, ev, 8);
    ASSERT_EQ_INT(&t, (int)ne, 0); /* x1 vs ~x1: no consistent cross pairs */
    ASSERT_EQ_INT(&t, sat3_decision(1, clauses, 2), 0);
    ASSERT_EQ_INT(&t, clique_decision(6, eu, ev, ne, 2), 0);
  }

  /* randomized equivalence: SAT(phi) <=> clique_decision(reduce(phi), m) */
  {
    srand(34u);
    for (int iter = 0; iter < 120; iter++) {
      int nclauses = clrs_rand_range_int(1, 4);
      int clauses[12];
      for (int c = 0; c < nclauses; c++) {
        for (int j = 0; j < 3; j++) {
          int v = clrs_rand_range_int(1, 3);
          clauses[c * 3 + j] = clrs_rand_range_int(0, 1) ? v : -v;
        }
      }
      int eu[64], ev[64];
      size_t ne = sat3_to_clique(3, clauses, nclauses, eu, ev, 64);
      int sat = sat3_decision(3, clauses, nclauses);
      int clique = clique_decision((size_t)(3 * nclauses), eu, ev, ne,
                                   nclauses);
      ASSERT_EQ_INT(&t, sat, clique);
    }
  }

  return test_report(&t, "np_complete");
}
