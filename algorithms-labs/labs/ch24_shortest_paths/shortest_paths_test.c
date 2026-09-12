#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "shortest_paths.h"
#include "test.h"

/* CLRS Fig 24.6: s=0, t=1, x=2, y=3, z=4
 * s-t 10, s-y 5, t-x 1, t-y 2, x-t 4, x-z 4, y-t 3, y-x 9, y-z 2, z-s 7, z-x 6
 * Dijkstra from s: d = [0, 8, 9, 5, 7] */
static void build_fig246(SPGraph *g) {
  spgraph_init(g, 5, 16);
  spgraph_add_edge(g, 0, 1, 10);
  spgraph_add_edge(g, 0, 3, 5);
  spgraph_add_edge(g, 1, 2, 1);
  spgraph_add_edge(g, 1, 3, 2);
  spgraph_add_edge(g, 2, 1, 4);
  spgraph_add_edge(g, 2, 4, 4);
  spgraph_add_edge(g, 3, 1, 3);
  spgraph_add_edge(g, 3, 2, 9);
  spgraph_add_edge(g, 3, 4, 2);
  spgraph_add_edge(g, 4, 0, 7);
  spgraph_add_edge(g, 4, 2, 6);
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* Bellman-Ford on Fig 24.1 (no negative cycle) */
  {
    SPGraph g;
    build_fig246(&g);
    long long dist[5];
    int parent[5];
    ASSERT_TRUE(&t, bellman_ford(&g, 0, dist, parent));
    ASSERT_EQ_INT(&t, (int)dist[0], 0);
    ASSERT_EQ_INT(&t, (int)dist[1], 8);
    ASSERT_EQ_INT(&t, (int)dist[2], 9);
    ASSERT_EQ_INT(&t, (int)dist[3], 5);
    ASSERT_EQ_INT(&t, (int)dist[4], 7);
    spgraph_destroy(&g);
  }

  /* negative edges OK, no cycle */
  {
    SPGraph g;
    spgraph_init(&g, 3, 8);
    spgraph_add_edge(&g, 0, 1, 5);
    spgraph_add_edge(&g, 0, 2, 10);
    spgraph_add_edge(&g, 1, 2, -8); /* path 0-1-2 = -3 < 10 */
    long long dist[3];
    int parent[3];
    ASSERT_TRUE(&t, bellman_ford(&g, 0, dist, parent));
    ASSERT_EQ_INT(&t, (int)dist[2], -3);
    spgraph_destroy(&g);
  }

  /* negative cycle detect */
  {
    SPGraph g;
    spgraph_init(&g, 3, 8);
    spgraph_add_edge(&g, 0, 1, 1);
    spgraph_add_edge(&g, 1, 2, -1);
    spgraph_add_edge(&g, 2, 1, -1); /* 1-2-1 weight -2 */
    long long dist[3];
    int parent[3];
    ASSERT_TRUE(&t, !bellman_ford(&g, 0, dist, parent));
    spgraph_destroy(&g);
  }

  /* Dijkstra Fig 24.6 */
  {
    SPGraph g;
    build_fig246(&g);
    long long dist[5];
    int parent[5];
    ASSERT_TRUE(&t, dijkstra(&g, 0, dist, parent));
    ASSERT_EQ_INT(&t, (int)dist[0], 0);
    ASSERT_EQ_INT(&t, (int)dist[1], 8);
    ASSERT_EQ_INT(&t, (int)dist[2], 9);
    ASSERT_EQ_INT(&t, (int)dist[3], 5);
    ASSERT_EQ_INT(&t, (int)dist[4], 7);

    int path[8];
    size_t pn = sp_path(0, 4, parent, path, 8);
    ASSERT_TRUE(&t, pn >= 2);
    ASSERT_EQ_INT(&t, path[0], 0);
    ASSERT_EQ_INT(&t, path[pn - 1], 4);
    spgraph_destroy(&g);
  }

  /* Dijkstra rejects negative weights */
  {
    SPGraph g;
    spgraph_init(&g, 2, 4);
    spgraph_add_edge(&g, 0, 1, -1);
    long long dist[2];
    int parent[2];
    ASSERT_TRUE(&t, !dijkstra(&g, 0, dist, parent));
    spgraph_destroy(&g);
  }

  /* DAG shortest paths: topo 0,1,2,3; edges 0->1 (1), 0->2 (5), 1->2 (2), 1->3 (6), 2->3 (1) */
  {
    SPGraph g;
    spgraph_init(&g, 4, 8);
    spgraph_add_edge(&g, 0, 1, 1);
    spgraph_add_edge(&g, 0, 2, 5);
    spgraph_add_edge(&g, 1, 2, 2);
    spgraph_add_edge(&g, 1, 3, 6);
    spgraph_add_edge(&g, 2, 3, 1);
    int topo[] = {0, 1, 2, 3};
    long long dist[4];
    int parent[4];
    dag_shortest_paths(&g, topo, 0, dist, parent);
    ASSERT_EQ_INT(&t, (int)dist[0], 0);
    ASSERT_EQ_INT(&t, (int)dist[1], 1);
    ASSERT_EQ_INT(&t, (int)dist[2], 3); /* 0-1-2 */
    ASSERT_EQ_INT(&t, (int)dist[3], 4); /* 0-1-2-3 */
    spgraph_destroy(&g);
  }

  /* unreachable */
  {
    SPGraph g;
    spgraph_init(&g, 3, 4);
    spgraph_add_edge(&g, 0, 1, 2);
    long long dist[3];
    int parent[3];
    dijkstra(&g, 0, dist, parent);
    ASSERT_TRUE(&t, dist[2] == LLONG_MAX);
    ASSERT_EQ_INT(&t, (int)dist[1], 2);
    ASSERT_TRUE(&t, bellman_ford(&g, 0, dist, parent));
    ASSERT_TRUE(&t, dist[2] == LLONG_MAX);
    int pdummy[1];
    ASSERT_EQ_INT(&t, (int)sp_path(0, 2, parent, pdummy, 1), 0);
    spgraph_destroy(&g);
  }

  /* DAG shortest paths with NEGATIVE weights (CLRS 24.2 selling point;
   * Dijkstra would refuse these) */
  {
    SPGraph g;
    spgraph_init(&g, 5, 8);
    spgraph_add_edge(&g, 0, 1, -3);
    spgraph_add_edge(&g, 0, 2, 4);
    spgraph_add_edge(&g, 1, 2, -2);
    spgraph_add_edge(&g, 1, 3, 5);
    spgraph_add_edge(&g, 2, 3, -1);
    spgraph_add_edge(&g, 3, 4, 2);
    int topo[] = {0, 1, 2, 3, 4};
    long long dist[5];
    int parent[5];
    dag_shortest_paths(&g, topo, 0, dist, parent);
    ASSERT_EQ_INT(&t, (int)dist[0], 0);
    ASSERT_EQ_INT(&t, (int)dist[1], -3);
    ASSERT_EQ_INT(&t, (int)dist[2], -5); /* 0-1-2 */
    ASSERT_EQ_INT(&t, (int)dist[3], -6); /* 0-1-2-3 */
    ASSERT_EQ_INT(&t, (int)dist[4], -4); /* 0-1-2-3-4 */
    int path[8];
    size_t pn = sp_path(0, 4, parent, path, 8);
    ASSERT_EQ_INT(&t, (int)pn, 5);
    ASSERT_EQ_INT(&t, path[0], 0);
    ASSERT_EQ_INT(&t, path[4], 4);
    /* Bellman-Ford agrees with the DAG result on negative weights */
    ASSERT_TRUE(&t, bellman_ford(&g, 0, dist, parent));
    ASSERT_EQ_INT(&t, (int)dist[4], -4);
    spgraph_destroy(&g);
  }

  return test_report(&t, "shortest_paths");
}
