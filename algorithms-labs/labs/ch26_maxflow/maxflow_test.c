#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "maxflow.h"
#include "test.h"

/* CLRS Fig 26.1: vertices s=0,v1=1,v2=2,v3=3,v4=4,t=5 max flow = 23 */
static void build_fig261(FlowGraph *g) {
  fg_init(g, 6);
  fg_set_capacity(g, 0, 1, 16);
  fg_set_capacity(g, 0, 2, 13);
  fg_set_capacity(g, 1, 2, 10);
  fg_set_capacity(g, 1, 3, 12);
  fg_set_capacity(g, 2, 1, 4);
  fg_set_capacity(g, 2, 4, 14);
  fg_set_capacity(g, 3, 2, 9);
  fg_set_capacity(g, 3, 5, 20);
  fg_set_capacity(g, 4, 3, 7);
  fg_set_capacity(g, 4, 5, 4);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Book figure 26.1 max flow = 23; verify a valid flow assignment:
   * capacities respected, conservation at every internal vertex, and the
   * net flow on the antiparallel pair (1,2)/(2,1) consistent */
  {
    FlowGraph g;
    build_fig261(&g);
    long long f = max_flow_edmonds_karp(&g, 0, 5);
    ASSERT_EQ_INT(&t, (int)f, 23);
    int caps[][3] = {{0, 1, 16}, {0, 2, 13}, {1, 2, 10}, {1, 3, 12},
                     {2, 1, 4},  {2, 4, 14}, {3, 2, 9},  {3, 5, 20},
                     {4, 3, 7},  {4, 5, 4}};
    long long balance[6] = {0};
    for (int i = 0; i < 10; i++) {
      int u = caps[i][0], v = caps[i][1];
      long long nf = fg_flow_on(&g, u, v);
      ASSERT_TRUE(&t, nf >= -caps[i][2]);
      ASSERT_TRUE(&t, nf <= caps[i][2]);
      balance[u] += nf; /* outflow positive */
      balance[v] -= nf;
    }
    ASSERT_EQ_INT(&t, (int)balance[0], 23); /* source net outflow */
    ASSERT_EQ_INT(&t, (int)balance[5], -23); /* sink net inflow */
    for (int v = 1; v <= 4; v++) {
      ASSERT_EQ_INT(&t, (int)balance[v], 0); /* conservation */
    }
    long long f12 = fg_flow_on(&g, 1, 2);
    ASSERT_EQ_INT(&t, (int)(f12 + fg_flow_on(&g, 2, 1)), 0); /* antisymmetry */
    ASSERT_TRUE(&t, f12 <= 10 && -f12 <= 4);
    fg_destroy(&g);
  }

  /* simple path */
  {
    FlowGraph g;
    fg_init(&g, 2);
    fg_set_capacity(&g, 0, 1, 10);
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 1), 10);
    fg_destroy(&g);
  }

  /* parallel edges via two paths */
  {
    FlowGraph g;
    fg_init(&g, 3);
    fg_set_capacity(&g, 0, 1, 5);
    fg_set_capacity(&g, 0, 2, 3);
    fg_set_capacity(&g, 1, 2, 2);
    fg_set_capacity(&g, 1, 2, 2); /* overwrite same edge */
    fg_set_capacity(&g, 1, 1, 0);
    /* rebuild: s->v1=5, s->v2=3, v1->v2=2, need v1->t, v2->t */
    fg_destroy(&g);
    fg_init(&g, 4);
    fg_set_capacity(&g, 0, 1, 5);
    fg_set_capacity(&g, 0, 2, 3);
    fg_set_capacity(&g, 1, 3, 4);
    fg_set_capacity(&g, 2, 3, 3);
    fg_set_capacity(&g, 1, 2, 2);
    /* max: s-t via 1 and 2: min(5+3, 4+3)=7? paths: s1t=4, s2t=3, leftover s1=1 can go 1-2-3 but 2-3 full with 3 from s2.
       s->1(4)->t, s->1(1)->2, s->2(3)->t total via t: 4+1+3 wait 2->t only 3 so s2(3) all to t.
       total = 4+3 = 7. Also s1 leftover 1 goes 1-2 but 2-t full. So 7. */
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 3), 7);
    fg_destroy(&g);
  }

  /* no path */
  {
    FlowGraph g;
    fg_init(&g, 3);
    fg_set_capacity(&g, 0, 1, 5);
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 2), 0);
    fg_destroy(&g);
  }

  /* min cut = max flow: edge s->t capacity 8 */
  {
    FlowGraph g;
    fg_init(&g, 2);
    fg_set_capacity(&g, 0, 1, 8);
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 1), 8);
    ASSERT_EQ_INT(&t, (int)fg_flow_on(&g, 0, 1), 8);
    ASSERT_EQ_INT(&t, (int)fg_flow_on(&g, 1, 0), -8);
    fg_destroy(&g);
  }

  /* antiparallel edges: net flows stay within each direction's capacity */
  {
    FlowGraph g;
    fg_init(&g, 3);
    fg_set_capacity(&g, 0, 1, 5);
    fg_set_capacity(&g, 1, 2, 3);
    fg_set_capacity(&g, 2, 1, 2); /* antiparallel with 1->2 */
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 2), 3);
    long long f12 = fg_flow_on(&g, 1, 2);
    ASSERT_TRUE(&t, f12 >= -2 && f12 <= 3);
    ASSERT_EQ_INT(&t, (int)(f12 + fg_flow_on(&g, 2, 1)), 0);
    fg_destroy(&g);
  }

  /* bottleneck in the middle */
  {
    FlowGraph g;
    fg_init(&g, 4);
    fg_set_capacity(&g, 0, 1, 100);
    fg_set_capacity(&g, 1, 2, 3);
    fg_set_capacity(&g, 2, 3, 100);
    ASSERT_EQ_INT(&t, (int)max_flow_edmonds_karp(&g, 0, 3), 3);
    fg_destroy(&g);
  }

  return test_report(&t, "maxflow");
}
