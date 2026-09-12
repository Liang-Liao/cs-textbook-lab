#include "maxflow.h"

#include <limits.h>
#include <stdlib.h>

#include "clrs.h"

void fg_init(FlowGraph *g, int V) {
  CLRS_ASSERT(V > 0, "V > 0");
  g->V = V;
  g->cap = clrs_xcalloc((size_t)V * (size_t)V, sizeof(long long));
  g->flow = clrs_xcalloc((size_t)V * (size_t)V, sizeof(long long));
}

void fg_destroy(FlowGraph *g) {
  free(g->cap);
  free(g->flow);
  g->cap = NULL;
  g->flow = NULL;
  g->V = 0;
}

void fg_set_capacity(FlowGraph *g, int u, int v, long long cap) {
  int V = g->V;
  CLRS_ASSERT(u >= 0 && u < V && v >= 0 && v < V, "vertex range");
  CLRS_ASSERT(cap >= 0, "capacity >= 0");
  CLRS_ASSERT(g->flow[u * V + v] == 0 && g->flow[v * V + u] == 0,
              "set capacities before running max flow");
  g->cap[u * V + v] = cap;
}

/* CLRS 26.2 Edmonds-Karp: BFS shortest augmenting path in residual. */
long long max_flow_edmonds_karp(FlowGraph *g, int s, int t) {
  int V = g->V;
  CLRS_ASSERT(s >= 0 && s < V && t >= 0 && t < V, "s/t out of range");
  CLRS_ASSERT(s != t, "source and sink must differ");
  long long *res = clrs_xmalloc((size_t)V * (size_t)V * sizeof(long long));
  for (int i = 0; i < V * V; i++) {
    res[i] = g->cap[i];
  }

  int *parent = clrs_xmalloc((size_t)V * sizeof(int));
  long long *bottle = clrs_xmalloc((size_t)V * sizeof(long long));
  int *queue = clrs_xmalloc((size_t)V * sizeof(int));
  long long maxflow = 0;

  for (;;) {
    for (int i = 0; i < V; i++) {
      parent[i] = -1;
    }
    parent[s] = s;
    bottle[s] = LLONG_MAX;

    size_t qh = 0, qt = 0;
    queue[qt++] = s;

    while (qh < qt && parent[t] < 0) {
      int u = queue[qh++];
      for (int v = 0; v < V; v++) {
        if (parent[v] < 0 && res[u * V + v] > 0) {
          parent[v] = u;
          long long b = bottle[u];
          if (res[u * V + v] < b) {
            b = res[u * V + v];
          }
          bottle[v] = b;
          queue[qt++] = v;
        }
      }
    }

    if (parent[t] < 0) {
      break; /* no augmenting path */
    }

    long long push = bottle[t];
    int v = t;
    while (v != s) {
      int u = parent[v];
      res[u * V + v] -= push;
      res[v * V + u] += push;
      g->flow[u * V + v] += push;
      g->flow[v * V + u] -= push;
      v = u;
    }
    maxflow += push;
  }

  /* write residual back so remaining capacity can be inspected */
  for (int i = 0; i < V * V; i++) {
    g->cap[i] = res[i];
  }

  free(parent);
  free(bottle);
  free(queue);
  free(res);
  return maxflow;
}

long long fg_flow_on(const FlowGraph *g, int u, int v) {
  int V = g->V;
  CLRS_ASSERT(u >= 0 && u < V && v >= 0 && v < V, "vertex range");
  return g->flow[u * V + v];
}
