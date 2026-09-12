#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "mst.h"
#include "test.h"

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Triangle: MST is two lightest edges */
  {
    WGraph g;
    wgraph_init(&g, 3, 8);
    wgraph_add_edge(&g, 0, 1, 1);
    wgraph_add_edge(&g, 1, 2, 2);
    wgraph_add_edge(&g, 0, 2, 3);
    WEdge mst[8];
    size_t k = mst_kruskal(&g, mst);
    ASSERT_EQ_INT(&t, (int)k, 2);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 3);
    k = mst_prim(&g, 0, mst);
    ASSERT_EQ_INT(&t, (int)k, 2);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 3);
    wgraph_destroy(&g);
  }

  /* CLRS Fig 23.1 graph: vertices a=0..i=8, MST weight 37 */
  {
    WGraph g;
    wgraph_init(&g, 9, 16);
    wgraph_add_edge(&g, 0, 1, 4);
    wgraph_add_edge(&g, 0, 7, 8);
    wgraph_add_edge(&g, 1, 2, 8);
    wgraph_add_edge(&g, 1, 7, 11);
    wgraph_add_edge(&g, 2, 3, 7);
    wgraph_add_edge(&g, 2, 5, 4);
    wgraph_add_edge(&g, 2, 8, 2);
    wgraph_add_edge(&g, 3, 4, 9);
    wgraph_add_edge(&g, 3, 5, 14);
    wgraph_add_edge(&g, 4, 5, 10);
    wgraph_add_edge(&g, 5, 6, 2);
    wgraph_add_edge(&g, 6, 7, 1);
    wgraph_add_edge(&g, 6, 8, 6);
    wgraph_add_edge(&g, 7, 8, 7);

    WEdge mst[16];
    size_t k = mst_kruskal(&g, mst);
    ASSERT_EQ_INT(&t, (int)k, 8);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 37);

    k = mst_prim(&g, 0, mst);
    ASSERT_EQ_INT(&t, (int)k, 8);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 37);

    k = mst_prim(&g, 4, mst);
    ASSERT_EQ_INT(&t, (int)k, 8);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 37);

    wgraph_destroy(&g);
  }

  /* single edge */
  {
    WGraph g;
    wgraph_init(&g, 2, 4);
    wgraph_add_edge(&g, 0, 1, 5);
    WEdge mst[4];
    size_t k = mst_kruskal(&g, mst);
    ASSERT_EQ_INT(&t, (int)k, 1);
    ASSERT_EQ_INT(&t, mst[0].w, 5);
    wgraph_destroy(&g);
  }

  /* equal weights */
  {
    WGraph g;
    wgraph_init(&g, 4, 8);
    wgraph_add_edge(&g, 0, 1, 1);
    wgraph_add_edge(&g, 1, 2, 1);
    wgraph_add_edge(&g, 2, 3, 1);
    wgraph_add_edge(&g, 0, 3, 1);
    wgraph_add_edge(&g, 0, 2, 2);
    wgraph_add_edge(&g, 1, 3, 2);
    WEdge mst[8];
    size_t k = mst_kruskal(&g, mst);
    ASSERT_EQ_INT(&t, (int)k, 3);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 3);
    k = mst_prim(&g, 2, mst);
    ASSERT_EQ_INT(&t, (int)k, 3);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 3);
    wgraph_destroy(&g);
  }

  /* disconnected graph: Kruskal returns a spanning forest (V - #components
   * edges); Prim returns only the component containing the root */
  {
    WGraph g;
    wgraph_init(&g, 4, 4);
    wgraph_add_edge(&g, 0, 1, 1);
    wgraph_add_edge(&g, 2, 3, 2);
    WEdge mst[4];
    size_t k = mst_kruskal(&g, mst);
    ASSERT_EQ_INT(&t, (int)k, 2);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 3);
    k = mst_prim(&g, 0, mst);
    ASSERT_EQ_INT(&t, (int)k, 1);
    ASSERT_EQ_INT(&t, (int)mst_weight(mst, k), 1);
    wgraph_destroy(&g);
  }

  return test_report(&t, "mst");
}
