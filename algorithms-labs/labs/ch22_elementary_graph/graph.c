#include "graph.h"

#include <stdlib.h>

#include "clrs.h"

void graph_init(Digraph *g, int V) {
  CLRS_ASSERT(V > 0, "V > 0");
  g->V = V;
  g->E = 0;
  g->adj = clrs_xcalloc((size_t)V, sizeof(EdgeNode *));
}

void graph_destroy(Digraph *g) {
  for (int i = 0; i < g->V; i++) {
    EdgeNode *e = g->adj[i];
    while (e != NULL) {
      EdgeNode *n = e->next;
      free(e);
      e = n;
    }
  }
  free(g->adj);
  g->adj = NULL;
  g->V = 0;
  g->E = 0;
}

void graph_add_edge(Digraph *g, int u, int v) {
  CLRS_ASSERT(u >= 0 && u < g->V && v >= 0 && v < g->V, "vertex out of range");
  EdgeNode *e = clrs_xmalloc(sizeof(EdgeNode));
  e->v = v;
  e->next = g->adj[u];
  g->adj[u] = e;
  g->E++;
}

void graph_transpose(const Digraph *g, Digraph *out) {
  graph_init(out, g->V);
  for (int u = 0; u < g->V; u++) {
    for (EdgeNode *e = g->adj[u]; e != NULL; e = e->next) {
      graph_add_edge(out, e->v, u);
    }
  }
}

void bfs(const Digraph *g, int s, int *parent, int *dist) {
  int V = g->V;
  CLRS_ASSERT(s >= 0 && s < V, "source out of range");
  int *color = clrs_xmalloc((size_t)V * sizeof(int)); /* 0 white 1 gray 2 black */
  int *queue = clrs_xmalloc((size_t)V * sizeof(int));

  for (int i = 0; i < V; i++) {
    color[i] = 0;
    parent[i] = -1;
    dist[i] = -1;
  }
  color[s] = 1;
  dist[s] = 0;
  parent[s] = -1;

  size_t qh = 0, qt = 0;
  queue[qt++] = s;
  while (qh < qt) {
    int u = queue[qh++];
    for (EdgeNode *e = g->adj[u]; e != NULL; e = e->next) {
      int v = e->v;
      if (color[v] == 0) {
        color[v] = 1;
        dist[v] = dist[u] + 1;
        parent[v] = u;
        queue[qt++] = v;
      }
    }
    color[u] = 2;
  }
  free(color);
  free(queue);
}

/* iterative DFS with explicit stack to avoid deep recursion */
static void dfs_visit(const Digraph *g, int s, int *color, int *disc, int *fin,
                      int *time, int *fin_order, int *fin_n) {
  int V = g->V;
  /* stack holds vertex and next-edge pointer as paired ints via index stack */
  typedef struct {
    int u;
    EdgeNode *e;
  } Frame;

  Frame *st = clrs_xmalloc((size_t)V * sizeof(Frame));
  size_t sp = 0;

  color[s] = 1;
  disc[s] = ++(*time);
  st[sp].u = s;
  st[sp].e = g->adj[s];
  sp++;

  while (sp > 0) {
    Frame *f = &st[sp - 1];
    int u = f->u;
    if (f->e != NULL) {
      int v = f->e->v;
      EdgeNode *next = f->e->next;
      f->e = next;
      if (color[v] == 0) {
        color[v] = 1;
        disc[v] = ++(*time);
        st[sp].u = v;
        st[sp].e = g->adj[v];
        sp++;
      }
    } else {
      color[u] = 2;
      fin[u] = ++(*time);
      if (fin_order != NULL) {
        fin_order[(*fin_n)++] = u;
      }
      sp--;
    }
  }
  free(st);
}

void dfs(const Digraph *g, int *disc, int *fin, int *fin_order) {
  int V = g->V;
  int *color = clrs_xcalloc((size_t)V, sizeof(int));
  int time = 0;
  int fin_n = 0;

  for (int i = 0; i < V; i++) {
    disc[i] = 0;
    fin[i] = 0;
  }
  for (int i = 0; i < V; i++) {
    if (color[i] == 0) {
      dfs_visit(g, i, color, disc, fin, &time, fin_order, &fin_n);
    }
  }
  free(color);
}

/* DFS that only records finish order (for topo / Kosaraju). */
static void dfs_finish_only(const Digraph *g, int s, int *color, int *order,
                            int *n) {
  int V = g->V;
  typedef struct {
    int u;
    EdgeNode *e;
  } Frame;
  Frame *st = clrs_xmalloc((size_t)V * sizeof(Frame));
  size_t sp = 0;

  color[s] = 1;
  st[sp].u = s;
  st[sp].e = g->adj[s];
  sp++;

  while (sp > 0) {
    Frame *f = &st[sp - 1];
    int u = f->u;
    if (f->e != NULL) {
      int v = f->e->v;
      f->e = f->e->next;
      if (color[v] == 0) {
        color[v] = 1;
        st[sp].u = v;
        st[sp].e = g->adj[v];
        sp++;
      }
    } else {
      color[u] = 2;
      order[(*n)++] = u;
      sp--;
    }
  }
  free(st);
}

int topo_sort(const Digraph *g, int *order) {
  int V = g->V;
  /* 0 white, 1 gray (on stack), 2 black */
  int *color = clrs_xcalloc((size_t)V, sizeof(int));
  int *fin_order = clrs_xmalloc((size_t)V * sizeof(int));
  int fin_n = 0;
  int cyclic = 0;

  for (int s = 0; s < V && !cyclic; s++) {
    if (color[s] != 0) {
      continue;
    }
    typedef struct {
      int u;
      EdgeNode *e;
    } Frame;
    Frame *st = clrs_xmalloc((size_t)V * sizeof(Frame));
    size_t sp = 0;
    color[s] = 1;
    st[sp].u = s;
    st[sp].e = g->adj[s];
    sp++;
    while (sp > 0) {
      Frame *f = &st[sp - 1];
      int u = f->u;
      if (f->e != NULL) {
        int v = f->e->v;
        f->e = f->e->next;
        if (color[v] == 1) {
          cyclic = 1; /* back edge */
          break;
        }
        if (color[v] == 0) {
          color[v] = 1;
          st[sp].u = v;
          st[sp].e = g->adj[v];
          sp++;
        }
      } else {
        color[u] = 2;
        fin_order[fin_n++] = u;
        sp--;
      }
    }
    free(st);
  }

  if (cyclic) {
    free(color);
    free(fin_order);
    return 0;
  }

  for (int i = 0; i < V; i++) {
    order[i] = fin_order[V - 1 - i];
  }
  free(color);
  free(fin_order);
  return 1;
}

int scc_kosaraju(const Digraph *g, int *comp) {
  int V = g->V;
  int *color = clrs_xcalloc((size_t)V, sizeof(int));
  int *order = clrs_xmalloc((size_t)V * sizeof(int));
  int n = 0;

  /* 1st DFS on G, record finish order */
  for (int i = 0; i < V; i++) {
    if (color[i] == 0) {
      dfs_finish_only(g, i, color, order, &n);
    }
  }

  Digraph gt;
  graph_transpose(g, &gt);

  /* 2nd DFS on G^T in decreasing finish time */
  free(color);
  color = clrs_xcalloc((size_t)V, sizeof(int));
  int k = 0;
  for (int i = V - 1; i >= 0; i--) {
    int u = order[i];
    if (color[u] == 0) {
      /* assign component k via a small DFS stack */
      typedef struct {
        int u;
        EdgeNode *e;
      } Frame;
      Frame *st = clrs_xmalloc((size_t)V * sizeof(Frame));
      size_t sp = 0;
      color[u] = 1;
      comp[u] = k;
      st[sp].u = u;
      st[sp].e = gt.adj[u];
      sp++;
      while (sp > 0) {
        Frame *f = &st[sp - 1];
        if (f->e != NULL) {
          int v = f->e->v;
          f->e = f->e->next;
          if (color[v] == 0) {
            color[v] = 1;
            comp[v] = k;
            st[sp].u = v;
            st[sp].e = gt.adj[v];
            sp++;
          }
        } else {
          sp--;
        }
      }
      free(st);
      k++;
    }
  }

  free(color);
  free(order);
  graph_destroy(&gt);
  return k;
}
