#include "all_pairs.h"

#include <stdlib.h>

#include "clrs.h"

void apsp_init_matrix(long long *W, size_t n) {
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      W[i * n + j] = (i == j) ? 0 : APSP_INF;
    }
  }
}

void apsp_set_edge(long long *W, size_t n, size_t u, size_t v, long long w) {
  W[u * n + v] = w;
}

/* CLRS 25.2 FLOYD-WARSHALL */
int floyd_warshall(const long long *W, size_t n, long long *dist_out) {
  for (size_t i = 0; i < n * n; i++) {
    dist_out[i] = W[i];
  }

  for (size_t k = 0; k < n; k++) {
    for (size_t i = 0; i < n; i++) {
      if (dist_out[i * n + k] == APSP_INF) {
        continue;
      }
      for (size_t j = 0; j < n; j++) {
        if (dist_out[k * n + j] == APSP_INF) {
          continue;
        }
        long long via = dist_out[i * n + k] + dist_out[k * n + j];
        if (via < dist_out[i * n + j]) {
          dist_out[i * n + j] = via;
        }
      }
    }
  }

  for (size_t i = 0; i < n; i++) {
    if (dist_out[i * n + i] < 0) {
      return 0;
    }
  }
  return 1;
}

/* Internal: Bellman-Ford from extra vertex s on n real vertices.
 * edges extracted from matrix. Returns 0 on negative cycle. */
static int bf_reweight(const long long *W, size_t n, long long *h) {
  /* Build edge list from matrix */
  size_t max_e = n * n;
  int *eu = clrs_xmalloc(max_e * sizeof(int));
  int *ev = clrs_xmalloc(max_e * sizeof(int));
  long long *ew = clrs_xmalloc(max_e * sizeof(long long));
  size_t m = 0;
  for (size_t u = 0; u < n; u++) {
    for (size_t v = 0; v < n; v++) {
      if (W[u * n + v] != APSP_INF && !(u == v && W[u * n + v] == 0 && n > 0)) {
        /* include all finite off-diagonal and possibly self-loops if finite */
        if (u != v || W[u * n + v] != 0) {
          eu[m] = (int)u;
          ev[m] = (int)v;
          ew[m] = W[u * n + v];
          m++;
        }
      }
    }
  }
  /* Also add s -> all v with weight 0 (virtual vertex n) */
  /* BF on vertices 0..n-1 with s = n, edges m + n */

  for (size_t i = 0; i < n; i++) {
    h[i] = 0; /* s->i = 0 already relaxed */
  }

  for (size_t iter = 0; iter < n; iter++) { /* V = n, one extra would be n+1 */
    int changed = 0;
    for (size_t e = 0; e < m; e++) {
      int u = eu[e];
      int v = ev[e];
      if (h[u] + ew[e] < h[v]) {
        h[v] = h[u] + ew[e];
        changed = 1;
      }
    }
    /* relax s->v: h[v] = min(h[v], 0) already 0 or negative after */
    if (!changed) {
      break;
    }
  }

  /* Check negative cycle */
  int neg = 0;
  for (size_t e = 0; e < m; e++) {
    if (h[eu[e]] + ew[e] < h[ev[e]]) {
      neg = 1;
      break;
    }
  }

  free(eu);
  free(ev);
  free(ew);
  return !neg;
}

/* Dijkstra on dense graph O(n^2) */
static void dijkstra_dense(const long long *W, size_t n, size_t src,
                           long long *dist) {
  int *done = clrs_xcalloc(n, sizeof(int));
  for (size_t i = 0; i < n; i++) {
    dist[i] = APSP_INF;
  }
  dist[src] = 0;

  for (size_t iter = 0; iter < n; iter++) {
    size_t u = (size_t)-1;
    long long best = APSP_INF;
    for (size_t i = 0; i < n; i++) {
      if (!done[i] && dist[i] < best) {
        best = dist[i];
        u = i;
      }
    }
    if (u == (size_t)-1 || best == APSP_INF) {
      break;
    }
    done[u] = 1;
    for (size_t v = 0; v < n; v++) {
      long long w = W[u * n + v];
      if (!done[v] && w != APSP_INF && dist[u] + w < dist[v]) {
        dist[v] = dist[u] + w;
      }
    }
  }
  free(done);
}

int johnson(const long long *W, size_t n, long long *dist_out) {
  long long *h = clrs_xmalloc(n * sizeof(long long));
  if (!bf_reweight(W, n, h)) {
    free(h);
    return 0;
  }

  /* reweighted W' */
  long long *Wp = clrs_xmalloc(n * n * sizeof(long long));
  for (size_t i = 0; i < n; i++) {
    for (size_t j = 0; j < n; j++) {
      if (W[i * n + j] == APSP_INF) {
        Wp[i * n + j] = APSP_INF;
      } else {
        Wp[i * n + j] = W[i * n + j] + h[i] - h[j];
      }
    }
  }

  long long *row = clrs_xmalloc(n * sizeof(long long));
  for (size_t u = 0; u < n; u++) {
    dijkstra_dense(Wp, n, u, row);
    for (size_t v = 0; v < n; v++) {
      if (row[v] == APSP_INF) {
        dist_out[u * n + v] = APSP_INF;
      } else {
        dist_out[u * n + v] = row[v] - h[u] + h[v];
      }
    }
  }

  free(row);
  free(Wp);
  free(h);
  return 1;
}
