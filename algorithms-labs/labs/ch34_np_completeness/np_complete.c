#include "np_complete.h"

#include <stdlib.h>

#include "clrs.h"

static void check_edges(size_t n, const int *eu, const int *ev, size_t m) {
  for (size_t e = 0; e < m; e++) {
    CLRS_ASSERT(eu[e] >= 0 && (size_t)eu[e] < n, "edge endpoint out of range");
    CLRS_ASSERT(ev[e] >= 0 && (size_t)ev[e] < n, "edge endpoint out of range");
  }
}

static int covers_all(size_t n, const int *eu, const int *ev, size_t m,
                      const unsigned char *in_set) {
  (void)n;
  for (size_t e = 0; e < m; e++) {
    if (!in_set[eu[e]] && !in_set[ev[e]]) {
      return 0;
    }
  }
  return 1;
}

/*
 * Enumerate subsets via bitmask when n <= 20; return -1 otherwise.
 * Vertex cover asks for a cover of size <= k, so k < 0 is never satisfiable.
 */
int vertex_cover_decision(size_t n, const int *eu, const int *ev, size_t m,
                          int k) {
  if (n > 20) {
    return -1; /* unsupported */
  }
  check_edges(n, eu, ev, m);
  if (k < 0) {
    return 0;
  }
  if (m == 0) {
    return 1;
  }
  unsigned char *in = clrs_xcalloc(n ? n : 1, sizeof(unsigned char));
  size_t lim = (size_t)1 << n;
  for (size_t mask = 0; mask < lim; mask++) {
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
      in[i] = (mask >> i) & 1u;
      bits += in[i];
    }
    if (bits <= k && covers_all(n, eu, ev, m, in)) {
      free(in);
      return 1;
    }
  }
  free(in);
  return 0;
}

/* Independent set of size >= k: k <= 0 is trivially satisfiable. */
int independent_set_decision(size_t n, const int *eu, const int *ev, size_t m,
                             int k) {
  if (n > 20) {
    return -1;
  }
  check_edges(n, eu, ev, m);
  if (k <= 0) {
    return 1;
  }
  unsigned char *in = clrs_xcalloc(n ? n : 1, sizeof(unsigned char));
  size_t lim = (size_t)1 << n;
  for (size_t mask = 0; mask < lim; mask++) {
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
      in[i] = (mask >> i) & 1u;
      bits += in[i];
    }
    if (bits < k) {
      continue;
    }
    int ok = 1;
    for (size_t e = 0; e < m; e++) {
      if (in[eu[e]] && in[ev[e]]) {
        ok = 0;
        break;
      }
    }
    if (ok) {
      free(in);
      return 1;
    }
  }
  free(in);
  return 0;
}

/* Clique of size >= k: k <= 0 is trivially satisfiable (n = 0 still fails
 * for k = 1, since no vertex exists). */
int clique_decision(size_t n, const int *eu, const int *ev, size_t m, int k) {
  if (n > 20) {
    return -1;
  }
  check_edges(n, eu, ev, m);
  if (k <= 0) {
    return 1;
  }
  if (k == 1) {
    return n >= 1 ? 1 : 0;
  }
  /* clique of size k iff complement has independent set of size k —
     brute force adjacency */
  unsigned char *adj = clrs_xcalloc(n * n, sizeof(unsigned char));
  for (size_t e = 0; e < m; e++) {
    int u = eu[e], v = ev[e];
    if (u != v) {
      adj[u * n + v] = 1;
      adj[v * n + u] = 1;
    }
  }
  unsigned char *in = clrs_xcalloc(n, sizeof(unsigned char));
  size_t lim = (size_t)1 << n;
  int found = 0;
  for (size_t mask = 0; mask < lim && !found; mask++) {
    int bits = 0;
    for (size_t i = 0; i < n; i++) {
      in[i] = (mask >> i) & 1u;
      bits += in[i];
    }
    if (bits < k) {
      continue;
    }
    int ok = 1;
    for (size_t i = 0; i < n && ok; i++) {
      if (!in[i]) {
        continue;
      }
      for (size_t j = i + 1; j < n; j++) {
        if (in[j] && !adj[i * n + j]) {
          ok = 0;
          break;
        }
      }
    }
    if (ok) {
      found = 1;
    }
  }
  free(adj);
  free(in);
  return found;
}

static int sat3_literal_true(int lit, const unsigned char *val) {
  int v = lit > 0 ? lit - 1 : (-lit) - 1;
  int t = val[v];
  return lit > 0 ? t : !t;
}

int sat3_decision(int nvars, const int *clauses, int nclauses) {
  if (nvars < 0 || nvars > 20) {
    return -1;
  }
  size_t lim = (size_t)1 << nvars;
  unsigned char *val = clrs_xcalloc(nvars ? nvars : 1, sizeof(unsigned char));
  for (size_t mask = 0; mask < lim; mask++) {
    for (int i = 0; i < nvars; i++) {
      val[i] = (mask >> i) & 1u;
    }
    int sat = 1;
    for (int c = 0; c < nclauses && sat; c++) {
      int ok = 0;
      for (int j = 0; j < 3; j++) {
        if (sat3_literal_true(clauses[c * 3 + j], val)) {
          ok = 1;
          break;
        }
      }
      if (!ok) {
        sat = 0;
      }
    }
    if (sat) {
      free(val);
      return 1;
    }
  }
  free(val);
  return 0;
}

/*
 * CLRS 34.5 reduction 3-SAT <=p CLIQUE (proof of Theorem 34.10).
 * Builds G: one vertex per literal occurrence, numbered 3*c + j for
 * clause c, literal j (so n = 3*nclauses). Edge between two vertices of
 * DIFFERENT clauses whose literals are consistent (not x with x).
 * phi is satisfiable  <=>  G has a clique of size nclauses.
 *
 * Writes edges into eu/ev (caller capacity max_edges) and returns the
 * number written; at most C(3m,2) - 3m edges are produced.
 */
size_t sat3_to_clique(int nvars, const int *clauses, int nclauses, int *eu,
                      int *ev, size_t max_edges) {
  CLRS_ASSERT(nvars >= 1 && nclauses >= 1, "empty formula");
  for (int c = 0; c < nclauses; c++) {
    for (int j = 0; j < 3; j++) {
      int lit = clauses[c * 3 + j];
      CLRS_ASSERT(lit != 0 && (lit > 0 ? lit : -lit) <= nvars,
                  "literal out of range");
    }
  }
  size_t k = 0;
  for (int c1 = 0; c1 < nclauses; c1++) {
    for (int j1 = 0; j1 < 3; j1++) {
      for (int c2 = c1 + 1; c2 < nclauses; c2++) {
        for (int j2 = 0; j2 < 3; j2++) {
          int l1 = clauses[c1 * 3 + j1];
          int l2 = clauses[c2 * 3 + j2];
          if (l1 != -l2) { /* consistent literals */
            CLRS_ASSERT(k < max_edges, "edge buffer too small");
            eu[k] = 3 * c1 + j1;
            ev[k] = 3 * c2 + j2;
            k++;
          }
        }
      }
    }
  }
  return k;
}
