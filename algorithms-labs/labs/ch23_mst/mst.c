#include "mst.h"

#include <limits.h>
#include <stdlib.h>

#include "clrs.h"

void wgraph_init(WGraph *g, int V, size_t max_edges) {
  CLRS_ASSERT(V > 0, "V > 0");
  g->V = V;
  g->E = 0;
  g->max_edges = max_edges;
  g->edges = clrs_xcalloc(max_edges, sizeof(WEdge));
}

void wgraph_destroy(WGraph *g) {
  free(g->edges);
  g->edges = NULL;
  g->V = 0;
  g->E = 0;
}

void wgraph_add_edge(WGraph *g, int u, int v, int w) {
  CLRS_ASSERT(u >= 0 && u < g->V && v >= 0 && v < g->V, "vertex range");
  CLRS_ASSERT(g->E < g->max_edges, "edge capacity exceeded");
  g->edges[g->E].u = u;
  g->edges[g->E].v = v;
  g->edges[g->E].w = w;
  g->E++;
}

long long mst_weight(const WEdge *edges, size_t n) {
  long long s = 0;
  for (size_t i = 0; i < n; i++) {
    s += edges[i].w;
  }
  return s;
}

/* ---- Union-Find (CLRS 21.3 style: parent + rank) ---- */

typedef struct {
  int *parent;
  int *rank;
} UF;

static void uf_init(UF *uf, int n) {
  uf->parent = clrs_xmalloc((size_t)n * sizeof(int));
  uf->rank = clrs_xcalloc((size_t)n, sizeof(int));
  for (int i = 0; i < n; i++) {
    uf->parent[i] = i;
  }
}

static void uf_destroy(UF *uf) {
  free(uf->parent);
  free(uf->rank);
}

static int uf_find(UF *uf, int x) {
  while (uf->parent[x] != x) {
    uf->parent[x] = uf->parent[uf->parent[x]];
    x = uf->parent[x];
  }
  return x;
}

static void uf_union(UF *uf, int x, int y) {
  int rx = uf_find(uf, x);
  int ry = uf_find(uf, y);
  if (rx == ry) {
    return;
  }
  if (uf->rank[rx] < uf->rank[ry]) {
    uf->parent[rx] = ry;
  } else if (uf->rank[rx] > uf->rank[ry]) {
    uf->parent[ry] = rx;
  } else {
    uf->parent[ry] = rx;
    uf->rank[rx]++;
  }
}

static int cmp_edge_w(const void *a, const void *b) {
  const WEdge *x = a;
  const WEdge *y = b;
  return (x->w > y->w) - (x->w < y->w);
}

/* CLRS 23.2 MST-KRUSKAL */
size_t mst_kruskal(const WGraph *g, WEdge *out) {
  WEdge *es = clrs_xmalloc(g->E * sizeof(WEdge));
  for (size_t i = 0; i < g->E; i++) {
    es[i] = g->edges[i];
  }
  qsort(es, g->E, sizeof(WEdge), cmp_edge_w);

  UF uf;
  uf_init(&uf, g->V);
  size_t k = 0;
  for (size_t i = 0; i < g->E && k + 1 < (size_t)g->V; i++) {
    int ru = uf_find(&uf, es[i].u);
    int rv = uf_find(&uf, es[i].v);
    if (ru != rv) {
      out[k++] = es[i];
      uf_union(&uf, ru, rv);
    }
  }
  uf_destroy(&uf);
  free(es);
  return k;
}

/* CLRS 23.2 MST-PRIM with array key/no heap. Adjacency is found by
 * scanning the edge list per extracted vertex: O(V*E) overall (O(V^2)
 * only for dense graphs). */
size_t mst_prim(const WGraph *g, int r, WEdge *out) {
  int V = g->V;
  CLRS_ASSERT(r >= 0 && r < V, "root out of range");
  int *key = clrs_xmalloc((size_t)V * sizeof(int));
  int *parent = clrs_xmalloc((size_t)V * sizeof(int));
  int *in_mst = clrs_xcalloc((size_t)V, sizeof(int));

  for (int i = 0; i < V; i++) {
    key[i] = INT_MAX;
    parent[i] = -1;
  }
  key[r] = 0;

  for (int iter = 0; iter < V; iter++) {
    int u = -1;
    int best = INT_MAX;
    for (int i = 0; i < V; i++) {
      if (!in_mst[i] && key[i] < best) {
        best = key[i];
        u = i;
      }
    }
    if (u < 0) {
      break;
    }
    in_mst[u] = 1;
    for (size_t e = 0; e < g->E; e++) {
      int a = g->edges[e].u;
      int b = g->edges[e].v;
      int w = g->edges[e].w;
      int v = -1;
      if (a == u) {
        v = b;
      } else if (b == u) {
        v = a;
      }
      if (v >= 0 && !in_mst[v] && w < key[v]) {
        key[v] = w;
        parent[v] = u;
      }
    }
  }

  size_t k = 0;
  for (int i = 0; i < V; i++) {
    if (parent[i] >= 0) {
      out[k].u = parent[i];
      out[k].v = i;
      out[k].w = key[i];
      k++;
    }
  }
  free(key);
  free(parent);
  free(in_mst);
  return k;
}
