#include "shortest_paths.h"

#include <limits.h>
#include <stdlib.h>

#include "clrs.h"

#define SP_INF LLONG_MAX

void spgraph_init(SPGraph *g, int V, size_t max_edges) {
  CLRS_ASSERT(V > 0, "V > 0");
  g->V = V;
  g->E = 0;
  g->max_edges = max_edges;
  g->edges = clrs_xcalloc(max_edges, sizeof(SPEdge));
}

void spgraph_destroy(SPGraph *g) {
  free(g->edges);
  g->edges = NULL;
  g->V = 0;
  g->E = 0;
}

void spgraph_add_edge(SPGraph *g, int u, int v, int w) {
  CLRS_ASSERT(u >= 0 && u < g->V && v >= 0 && v < g->V, "vertex range");
  CLRS_ASSERT(g->E < g->max_edges, "edge capacity exceeded");
  g->edges[g->E].u = u;
  g->edges[g->E].v = v;
  g->edges[g->E].w = w;
  g->E++;
}

static void init_ss(const SPGraph *g, int s, long long *dist, int *parent) {
  for (int i = 0; i < g->V; i++) {
    dist[i] = SP_INF;
    parent[i] = -1;
  }
  dist[s] = 0;
}

/* CLRS 24.1 BELLMAN-FORD */
int bellman_ford(const SPGraph *g, int s, long long *dist, int *parent) {
  init_ss(g, s, dist, parent);
  int V = g->V;

  for (int i = 1; i < V; i++) {
    for (size_t e = 0; e < g->E; e++) {
      int u = g->edges[e].u;
      int v = g->edges[e].v;
      int w = g->edges[e].w;
      if (dist[u] != SP_INF && dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        parent[v] = u;
      }
    }
  }

  for (size_t e = 0; e < g->E; e++) {
    int u = g->edges[e].u;
    int v = g->edges[e].v;
    int w = g->edges[e].w;
    if (dist[u] != SP_INF && dist[u] + w < dist[v]) {
      return 0; /* negative cycle */
    }
  }
  return 1;
}

/* CLRS 24.2 DAG-SHORTEST-PATHS */
void dag_shortest_paths(const SPGraph *g, const int *topo, int s, long long *dist,
                        int *parent) {
  init_ss(g, s, dist, parent);
  for (int i = 0; i < g->V; i++) {
    int u = topo[i];
    if (dist[u] == SP_INF) {
      continue;
    }
    for (size_t e = 0; e < g->E; e++) {
      if (g->edges[e].u != u) {
        continue;
      }
      int v = g->edges[e].v;
      int w = g->edges[e].w;
      if (dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        parent[v] = u;
      }
    }
  }
}

/* CLRS 24.3 DIJKSTRA — O(V^2) */
int dijkstra(const SPGraph *g, int s, long long *dist, int *parent) {
  for (size_t e = 0; e < g->E; e++) {
    if (g->edges[e].w < 0) {
      return 0;
    }
  }
  int V = g->V;
  init_ss(g, s, dist, parent);
  int *done = clrs_xcalloc((size_t)V, sizeof(int));

  for (int iter = 0; iter < V; iter++) {
    int u = -1;
    long long best = SP_INF;
    for (int i = 0; i < V; i++) {
      if (!done[i] && dist[i] < best) {
        best = dist[i];
        u = i;
      }
    }
    if (u < 0 || best == SP_INF) {
      break;
    }
    done[u] = 1;
    for (size_t e = 0; e < g->E; e++) {
      if (g->edges[e].u != u) {
        continue;
      }
      int v = g->edges[e].v;
      int w = g->edges[e].w;
      if (!done[v] && dist[u] != SP_INF && dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
        parent[v] = u;
      }
    }
  }
  free(done);
  return 1;
}

size_t sp_path(int s, int v, const int *parent, int *path_out,
               size_t max_cap) {
  if (max_cap == 0) {
    return 0;
  }
  if (v == s) {
    path_out[0] = s;
    return 1;
  }
  if (parent[v] < 0) {
    return 0; /* unreachable: parent is unset */
  }
  int *rev = clrs_xmalloc(max_cap * sizeof(int));
  size_t n = 0;
  int x = v;
  while (x != s && x >= 0 && n < max_cap) {
    rev[n++] = x;
    x = parent[x];
  }
  if (x != s || n == max_cap) {
    free(rev);
    return 0;
  }
  rev[n++] = s;
  for (size_t i = 0; i < n; i++) {
    path_out[i] = rev[n - 1 - i];
  }
  free(rev);
  return n;
}
