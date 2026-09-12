#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clrs.h"
#include "dp.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book rod prices p1..p10 */
  {
    int price[] = {1, 5, 8, 9, 10, 17, 17, 20, 24, 30};
    ASSERT_EQ_INT(&t, (int)rod_cut_bottom_up(price, 1), 1);
    ASSERT_EQ_INT(&t, (int)rod_cut_bottom_up(price, 5), 13); /* 2+3 */
    ASSERT_EQ_INT(&t, (int)rod_cut_bottom_up(price, 10), 30);
    ASSERT_EQ_INT(&t, (int)rod_cut_memo(price, 5), 13);
    ASSERT_EQ_INT(&t, (int)rod_cut_memo(price, 10), 30);

    /* n=4: max is 2+2 = 10? prices: 1,5,8,9 → 4=9, 2+2=10, 1+3=9 → 10 */
    ASSERT_EQ_INT(&t, (int)rod_cut_bottom_up(price, 4), 10);
    ASSERT_EQ_INT(&t, (int)rod_cut_memo(price, 4), 10);
  }

  {
    int price[] = {3, 5};
    ASSERT_EQ_INT(&t, (int)rod_cut_bottom_up(price, 2), 6); /* two pieces of 1 */
    ASSERT_EQ_INT(&t, (int)rod_cut_memo(price, 1), 3);
  }

  /* Matrix chain CLRS 15.3-4 style: A1 10x30, A2 30x5, A3 5x60
   * min cost = 1500 (A2 A3 first? actually ((A1 A2) A3) = 10*30*5+10*5*60=4500;
   * (A1 (A2 A3)) = 30*5*60+10*30*60=27000; wait book classic 3 matrices 10,30,5,60:
   * ((A1A2)A3)=10*30*5 + 10*5*60 = 1500+3000=4500
   * (A1(A2A3))=30*5*60 + 10*30*60 = 9000+18000=27000
   * min = 4500 */
  {
    int dims[] = {10, 30, 5, 60};
    ASSERT_EQ_INT(&t, (int)matrix_chain_min(dims, 3, NULL), 4500);
  }

  /* Book p.337 example: dims 30,35,15,5,10,20,25 → 15125 */
  {
    int dims[] = {30, 35, 15, 5, 10, 20, 25};
    ASSERT_EQ_INT(&t, (int)matrix_chain_min(dims, 6, NULL), 15125);
  }

  {
    int dims[] = {5, 5};
    ASSERT_EQ_INT(&t, (int)matrix_chain_min(dims, 1, NULL), 0);
  }

  /* LCS book: X = ABCBDAB, Y = BDCABA → LCS length 4 (BCBA or BDAB) */
  {
    char lcs[32];
    size_t len = lcs_length("ABCBDAB", "BDCABA", lcs, 32);
    ASSERT_EQ_INT(&t, (int)len, 4);
    ASSERT_EQ_INT(&t, (int)strlen(lcs), 4);
    /* verify lcs is subsequence of both */
    /* simple: known possible LCS "BCBA" or "BDAB" */
    ASSERT_TRUE(&t, strcmp(lcs, "BCBA") == 0 || strcmp(lcs, "BDAB") == 0 ||
                        strcmp(lcs, "BCBA") == 0);
  }

  {
    ASSERT_EQ_INT(&t, (int)lcs_length("abc", "abc", NULL, 0), 3);
    ASSERT_EQ_INT(&t, (int)lcs_length("abc", "xyz", NULL, 0), 0);
    ASSERT_EQ_INT(&t, (int)lcs_length("", "abc", NULL, 0), 0);
  }

  {
    char lcs[16];
    size_t len = lcs_length("AGGTAB", "GXTXAYB", lcs, 16);
    ASSERT_EQ_INT(&t, (int)len, 4); /* GTAB */
    ASSERT_TRUE(&t, strcmp(lcs, "GTAB") == 0);
  }

  /* CLRS 15.5 example: cost 2.75, root k2 = index 1 */
  {
    double p[] = {0.15, 0.10, 0.05, 0.10, 0.20};
    double q[] = {0.05, 0.10, 0.05, 0.05, 0.05, 0.10};
    int root[25];
    double cost = optimal_bst(p, q, 5, root);
    ASSERT_TRUE(&t, fabs(cost - 2.75) < 1e-9);
    ASSERT_EQ_INT(&t, root[0 * 5 + 4], 1);
  }

  /* single key: e = q0 + q1 + (q0+p0+q1) = p0 + 2(q0+q1) */
  {
    double p[] = {0.5};
    double q[] = {0.25, 0.25};
    double cost = optimal_bst(p, q, 1, NULL);
    ASSERT_TRUE(&t, fabs(cost - 1.5) < 1e-9);
  }

  /* two keys equal p: either root ok; cost = p0+p1 + left dummy weight chain */
  {
    double p[] = {0.5, 0.5};
    double q[] = {0.0, 0.0, 0.0};
    double cost = optimal_bst(p, q, 2, NULL);
    /* root k0: w=1, left empty 0, right k1: w=p1 + ... e[k1]=0 (dummies 0)
       t = 0 + 0 + 1 + then? e[0,2] = min over r
       r=0: e[0,0]+e[1,2]+w = 0 + (p1) + (p0+p1) = 0.5 + 1 = 1.5?
       e[1,2] for keys 1 only = p1 + dummies = 0.5
       w[0,2] = p0+p1 = 1
       r=0: 0+0.5+1=1.5; r=1: e[0,1]+e[2,2]+1 = 0.5+0+1=1.5
    */
    ASSERT_TRUE(&t, fabs(cost - 1.5) < 1e-9);
  }

  return test_report(&t, "dynamic_programming");
}
