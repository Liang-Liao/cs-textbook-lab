#include "approx.h"

#include <float.h>
#include <stdlib.h>

#include "clrs.h"

/* CLRS 35.1 APPROX-VERTEX-COVER */
size_t vertex_cover_approx(size_t n, const int *eu, const int *ev, size_t m,
                           int *cover_out, size_t max_cover) {
  for (size_t e = 0; e < m; e++) {
    CLRS_ASSERT(eu[e] >= 0 && (size_t)eu[e] < n, "edge endpoint out of range");
    CLRS_ASSERT(ev[e] >= 0 && (size_t)ev[e] < n, "edge endpoint out of range");
  }
  unsigned char *taken = clrs_xcalloc(m > 0 ? m : 1, sizeof(unsigned char));
  unsigned char *picked = clrs_xcalloc(n > 0 ? n : 1, sizeof(unsigned char));
  size_t cover_n = 0;

  for (size_t e = 0; e < m; e++) {
    if (taken[e]) {
      continue;
    }
    int u = eu[e];
    int v = ev[e];
    if (!picked[u]) {
      if (cover_out != NULL && cover_n < max_cover) {
        cover_out[cover_n] = u;
      }
      cover_n++;
      picked[u] = 1;
    }
    if (!picked[v]) {
      if (cover_out != NULL && cover_n < max_cover) {
        cover_out[cover_n] = v;
      }
      cover_n++;
      picked[v] = 1;
    }
    /* mark all edges incident to u or v */
    for (size_t j = e; j < m; j++) {
      if (eu[j] == u || eu[j] == v || ev[j] == u || ev[j] == v) {
        taken[j] = 1;
      }
    }
  }

  free(taken);
  free(picked);
  return cover_n;
}

/* Prim MST on dense matrix; parent[] and key from root 0. */
static void mst_prim(const double *dist, size_t n, int *parent) {
  double *key = clrs_xmalloc(n * sizeof(double));
  int *in = clrs_xcalloc(n, sizeof(int));
  for (size_t i = 0; i < n; i++) {
    key[i] = DBL_MAX;
    parent[i] = -1;
  }
  key[0] = 0.0;
  for (size_t it = 0; it < n; it++) {
    size_t u = (size_t)-1;
    double best = DBL_MAX;
    for (size_t i = 0; i < n; i++) {
      if (!in[i] && key[i] < best) {
        best = key[i];
        u = i;
      }
    }
    if (u == (size_t)-1) {
      break;
    }
    in[u] = 1;
    for (size_t v = 0; v < n; v++) {
      if (!in[v] && dist[u * n + v] < key[v]) {
        key[v] = dist[u * n + v];
        parent[v] = (int)u;
      }
    }
  }
  free(key);
  free(in);
}

static void dfs_preorder(int u, const int *parent, size_t n, int *order,
                         size_t *k, int *seen) {
  order[(*k)++] = u;
  seen[u] = 1;
  /* children of u: parent[c]==u */
  for (size_t v = 0; v < n; v++) {
    if (parent[v] == u && !seen[v]) {
      dfs_preorder((int)v, parent, n, order, k, seen);
    }
  }
}

double tsp_metric_approx(const double *dist, size_t n, int *tour_out) {
  if (n == 0) {
    return -1.0;
  }
  if (n == 1) {
    tour_out[0] = 0;
    return 0.0;
  }

  int *parent = clrs_xmalloc(n * sizeof(int));
  mst_prim(dist, n, parent);

  int *seen = clrs_xcalloc(n, sizeof(int));
  size_t k = 0;
  dfs_preorder(0, parent, n, tour_out, &k, seen);
  /* if some vertex unreachable (shouldn't for metric connected), append */
  for (size_t i = 0; i < n && k < n; i++) {
    if (!seen[i]) {
      tour_out[k++] = (int)i;
    }
  }

  double cost = 0.0;
  for (size_t i = 0; i < n; i++) {
    int a = tour_out[i];
    int b = tour_out[(i + 1) % n];
    cost += dist[(size_t)a * n + (size_t)b];
  }

  free(parent);
  free(seen);
  return cost;
}
