#include "dp.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "clrs.h"

/* ---- Rod cutting (CLRS 15.1) ---- */

long long rod_cut_bottom_up(const int *price, int n) {
  CLRS_ASSERT(n >= 0, "n >= 0");
  long long *r = clrs_xcalloc((size_t)n + 1, sizeof(long long));
  for (int j = 1; j <= n; j++) {
    long long q = LLONG_MIN;
    for (int i = 1; i <= j; i++) {
      long long cand = (long long)price[i - 1] + r[j - i];
      if (cand > q) {
        q = cand;
      }
    }
    r[j] = q;
  }
  long long ans = r[n];
  free(r);
  return ans;
}

static long long rod_cut_aux(const int *price, int n, long long *r) {
  if (r[n] >= 0) {
    return r[n];
  }
  long long q = (n == 0) ? 0 : LLONG_MIN;
  for (int i = 1; i <= n; i++) {
    long long cand = (long long)price[i - 1] + rod_cut_aux(price, n - i, r);
    if (cand > q) {
      q = cand;
    }
  }
  r[n] = q;
  return q;
}

long long rod_cut_memo(const int *price, int n) {
  CLRS_ASSERT(n >= 0, "n >= 0");
  long long *r = clrs_xmalloc(((size_t)n + 1) * sizeof(long long));
  for (int i = 0; i <= n; i++) {
    r[i] = LLONG_MIN;
  }
  r[0] = 0;
  long long ans = rod_cut_aux(price, n, r);
  free(r);
  return ans;
}

/* ---- Matrix-chain (CLRS 15.2) ---- */

long long matrix_chain_min(const int *dims, size_t n, int *splits) {
  /* n matrices => dims has n+1 entries */
  CLRS_ASSERT(n >= 1, "need at least 1 matrix");
  size_t N = n;

  long long *mm = clrs_xmalloc(N * N * sizeof(long long));
  int *ss = (splits != NULL) ? splits : clrs_xcalloc(N * N, sizeof(int));
  int own_ss = (splits == NULL);

  for (size_t i = 0; i < N * N; i++) {
    mm[i] = 0;
    ss[i] = 0;
  }

  for (size_t l = 2; l <= N; l++) { /* chain length */
    for (size_t i = 0; i + l <= N; i++) {
      size_t j = i + l - 1;
      mm[i * N + j] = LLONG_MAX;
      for (size_t k = i; k < j; k++) {
        /* A(i..k) * A(k+1..j) + dims[i]*dims[k+1]*dims[j+1] */
        long long q = mm[i * N + k] + mm[(k + 1) * N + j] +
                      (long long)dims[i] * dims[k + 1] * dims[j + 1];
        if (q < mm[i * N + j]) {
          mm[i * N + j] = q;
          ss[i * N + j] = (int)k;
        }
      }
    }
  }

  long long ans = mm[0 * N + (N - 1)];
  free(mm);
  if (own_ss) {
    free(ss);
  }
  return ans;
}

/* ---- LCS (CLRS 15.4) ---- */

size_t lcs_length(const char *x, const char *y, char *lcs_out, size_t maxn) {
  size_t m = strlen(x);
  size_t n = strlen(y);

  size_t *c = clrs_xcalloc((m + 1) * (n + 1), sizeof(size_t));
  unsigned char *b = clrs_xcalloc((m + 1) * (n + 1), sizeof(unsigned char));
  /* b[i,j]: 0 = up-left match, 1 = up, 2 = left */
#define C(i, j) c[(i) * (n + 1) + (j)]
#define B(i, j) b[(i) * (n + 1) + (j)]

  for (size_t i = 1; i <= m; i++) {
    for (size_t j = 1; j <= n; j++) {
      if (x[i - 1] == y[j - 1]) {
        C(i, j) = C(i - 1, j - 1) + 1;
        B(i, j) = 0;
      } else if (C(i - 1, j) >= C(i, j - 1)) {
        C(i, j) = C(i - 1, j);
        B(i, j) = 1;
      } else {
        C(i, j) = C(i, j - 1);
        B(i, j) = 2;
      }
    }
  }

  size_t len = C(m, n);
  if (lcs_out != NULL && maxn > 0) {
    size_t i = m, j = n, p = 0;
    char *rev = clrs_xmalloc((len + 1) * sizeof(char));
    while (i > 0 && j > 0) {
      if (B(i, j) == 0) {
        rev[p++] = x[i - 1];
        i--;
        j--;
      } else if (B(i, j) == 1) {
        i--;
      } else {
        j--;
      }
    }
    size_t w = 0;
    for (size_t t = 0; t < p && w + 1 < maxn; t++) {
      lcs_out[w++] = rev[p - 1 - t];
    }
    lcs_out[w] = '\0';
    free(rev);
  }

#undef C
#undef B
  free(c);
  free(b);
  return len;
}

/*
 * CLRS 15.5 OPTIMAL-BST.
 * 0-based keys 0..n-1, dummy probabilities q[0..n].
 * Half-open DP: e[i][j] = min cost of keys i..j-1 (empty when i==j).
 */
double optimal_bst(const double *p, const double *q, size_t n, int *root_out) {
  if (n == 0) {
    return (q != NULL) ? q[0] : 0.0;
  }
  size_t N = n + 1;
  double *e = clrs_xmalloc(N * N * sizeof(double));
  double *w = clrs_xmalloc(N * N * sizeof(double));
  int *root = clrs_xmalloc(N * N * sizeof(int));
#define E(i, j) e[(size_t)(i) * N + (size_t)(j)]
#define W(i, j) w[(size_t)(i) * N + (size_t)(j)]
#define R(i, j) root[(size_t)(i) * N + (size_t)(j)]

  for (size_t i = 0; i <= n; i++) {
    E(i, i) = q[i];
    W(i, i) = q[i];
  }

  for (size_t len = 1; len <= n; len++) {
    for (size_t i = 0; i + len <= n; i++) {
      size_t j = i + len;
      E(i, j) = 1e300;
      W(i, j) = W(i, j - 1) + p[j - 1] + q[j];
      for (size_t r = i; r < j; r++) {
        double t = E(i, r) + E(r + 1, j) + W(i, j);
        if (t < E(i, j)) {
          E(i, j) = t;
          R(i, j) = (int)r;
        }
      }
    }
  }

  double ans = E(0, n);
  if (root_out != NULL) {
    for (size_t i = 0; i < n; i++) {
      for (size_t j = 0; j < n; j++) {
        root_out[i * n + j] = (j >= i) ? R(i, j + 1) : -1;
      }
    }
  }

#undef E
#undef W
#undef R
  free(e);
  free(w);
  free(root);
  return ans;
}
