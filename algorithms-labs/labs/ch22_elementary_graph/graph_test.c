#include <stdio.h>
#include <stdlib.h>

#include "clrs.h"
#include "graph.h"
#include "test.h"

/* CLRS Fig 22.3 style DAG for BFS: undirected feel via two directed edges */
static void build_bfs_demo(Digraph *g) {
  graph_init(g, 6);
  /* r-s, r-v as two-way for undirected BFS demo */
  graph_add_edge(g, 0, 1); /* r->s */
  graph_add_edge(g, 1, 0);
  graph_add_edge(g, 0, 2); /* r->v */
  graph_add_edge(g, 2, 0);
  graph_add_edge(g, 1, 2); /* s->v */
  graph_add_edge(g, 2, 1);
  graph_add_edge(g, 1, 3); /* s->w */
  graph_add_edge(g, 3, 1);
  graph_add_edge(g, 3, 2); /* w->v */
  graph_add_edge(g, 2, 3);
  graph_add_edge(g, 3, 4); /* w->x */
  graph_add_edge(g, 4, 3);
  graph_add_edge(g, 4, 5); /* x->y */
  graph_add_edge(g, 5, 4);
  graph_add_edge(g, 2, 5); /* v->y */
  graph_add_edge(g, 5, 2);
}

/* CLRS Fig 22.9 SCC example */
static void build_scc_graph(Digraph *g) {
  graph_init(g, 8);
  graph_add_edge(g, 0, 1); /* a->b */
  graph_add_edge(g, 1, 2); /* b->c */
  graph_add_edge(g, 2, 3); /* c->d */
  graph_add_edge(g, 3, 2); /* d->c */
  graph_add_edge(g, 3, 4); /* d->e */
  graph_add_edge(g, 4, 5); /* e->f */
  graph_add_edge(g, 5, 4); /* f->e */
  graph_add_edge(g, 5, 6); /* f->g */
  graph_add_edge(g, 6, 6); /* g->g */
  graph_add_edge(g, 6, 7); /* g->h */
  graph_add_edge(g, 7, 6); /* h->g */
  graph_add_edge(g, 7, 5); /* h->f */
  graph_add_edge(g, 1, 5); /* b->f */
  graph_add_edge(g, 1, 7); /* b->h */
  graph_add_edge(g, 0, 4); /* a->e */
}

int main(void) {
  TestSuite t;
  test_init(&t);

  /* --- BFS --- */
  {
    Digraph g;
    build_bfs_demo(&g);
    int parent[6], dist[6];
    bfs(&g, 0, parent, dist); /* from r=0 */
    ASSERT_EQ_INT(&t, dist[0], 0);
    ASSERT_EQ_INT(&t, dist[1], 1); /* s */
    ASSERT_EQ_INT(&t, dist[2], 1); /* v */
    ASSERT_EQ_INT(&t, dist[3], 2); /* w */
    ASSERT_EQ_INT(&t, dist[4], 3); /* x */
    ASSERT_EQ_INT(&t, dist[5], 2); /* y via v */
    graph_destroy(&g);
  }

  {
    Digraph g;
    graph_init(&g, 3);
    graph_add_edge(&g, 0, 1);
    int parent[3], dist[3];
    bfs(&g, 2, parent, dist); /* isolated */
    ASSERT_EQ_INT(&t, dist[2], 0);
    ASSERT_EQ_INT(&t, dist[0], -1);
    ASSERT_EQ_INT(&t, dist[1], -1);
    graph_destroy(&g);
  }

  /* --- DFS discovery/finish ordering sanity --- */
  {
    Digraph g;
    graph_init(&g, 3);
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 1, 2);
    int disc[3], fin[3], order[3];
    dfs(&g, disc, fin, order);
    ASSERT_TRUE(&t, disc[0] < fin[0]);
    ASSERT_TRUE(&t, disc[0] < disc[1]);
    ASSERT_TRUE(&t, disc[1] < disc[2]);
    ASSERT_TRUE(&t, fin[2] < fin[1] && fin[1] < fin[0]); /* nested path */
    graph_destroy(&g);
  }

  /* --- topological sort on DAG --- */
  {
    /* edges: 5->2, 5->0, 4->0, 4->1, 2->3, 3->1 */
    Digraph g;
    graph_init(&g, 6);
    graph_add_edge(&g, 5, 2);
    graph_add_edge(&g, 5, 0);
    graph_add_edge(&g, 4, 0);
    graph_add_edge(&g, 4, 1);
    graph_add_edge(&g, 2, 3);
    graph_add_edge(&g, 3, 1);
    int order[6];
    ASSERT_TRUE(&t, topo_sort(&g, order));

    /* position map */
    int pos[6];
    for (int i = 0; i < 6; i++) {
      pos[order[i]] = i;
    }
    ASSERT_TRUE(&t, pos[5] < pos[2]);
    ASSERT_TRUE(&t, pos[5] < pos[0]);
    ASSERT_TRUE(&t, pos[4] < pos[0]);
    ASSERT_TRUE(&t, pos[4] < pos[1]);
    ASSERT_TRUE(&t, pos[2] < pos[3]);
    ASSERT_TRUE(&t, pos[3] < pos[1]);
    graph_destroy(&g);
  }

  /* cycle detection */
  {
    Digraph g;
    graph_init(&g, 3);
    graph_add_edge(&g, 0, 1);
    graph_add_edge(&g, 1, 2);
    graph_add_edge(&g, 2, 0);
    int order[3];
    ASSERT_TRUE(&t, !topo_sort(&g, order));
    graph_destroy(&g);
  }
  {
    /* self-loop */
    Digraph g;
    graph_init(&g, 2);
    graph_add_edge(&g, 0, 0);
    int order[2];
    ASSERT_TRUE(&t, !topo_sort(&g, order));
    graph_destroy(&g);
  }

  /* --- Kosaraju SCC ---
   * Our edges: a->b, b->c, c<->d, d->e, e<->f, f->g, g<->h, h->f
   *            b->f, b->h, a->e
   * SCCs: {a}, {b}, {c,d}, {e,f,g,h}  — 4 components */
  {
    Digraph g;
    build_scc_graph(&g);
    int comp[8];
    int k = scc_kosaraju(&g, comp);
    ASSERT_EQ_INT(&t, k, 4);
    ASSERT_TRUE(&t, comp[2] == comp[3]); /* c,d */
    ASSERT_TRUE(&t, comp[4] == comp[5] && comp[5] == comp[6] &&
                        comp[6] == comp[7]); /* e,f,g,h */
    ASSERT_TRUE(&t, comp[0] != comp[1]);
    ASSERT_TRUE(&t, comp[0] != comp[2]);
    ASSERT_TRUE(&t, comp[1] != comp[2]);
    ASSERT_TRUE(&t, comp[1] != comp[4]);
    ASSERT_TRUE(&t, comp[3] != comp[4]);
    graph_destroy(&g);
  }

  /* transpose */
  {
    Digraph g, gt;
    graph_init(&g, 2);
    graph_add_edge(&g, 0, 1);
    graph_transpose(&g, &gt);
    ASSERT_EQ_INT(&t, (int)gt.E, 1);
    ASSERT_EQ_INT(&t, gt.adj[1]->v, 0);
    graph_destroy(&g);
    graph_destroy(&gt);
  }

  return test_report(&t, "elementary_graph");
}
