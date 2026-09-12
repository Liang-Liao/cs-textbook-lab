#include "greedy.h"

#include <stdlib.h>

#include "clrs.h"

static int cmp_activity_finish(const void *a, const void *b) {
  const Activity *x = a;
  const Activity *y = b;
  return (x->finish > y->finish) - (x->finish < y->finish);
}

/*
 * CLRS RECURSIVE-ACTIVITY-SELECTOR / iterative greedy:
 * sort by finish time, then repeatedly take the next compatible activity.
 */
size_t activity_select(Activity *acts, size_t n, size_t *selected_out) {
  if (n == 0) {
    return 0;
  }
  /* stable copy of indices after sort — sort acts in place */
  qsort(acts, n, sizeof(Activity), cmp_activity_finish);

  size_t count = 0;
  size_t last = 0;
  selected_out[count++] = 0;
  for (size_t i = 1; i < n; i++) {
    if (acts[i].start >= acts[last].finish) {
      selected_out[count++] = i;
      last = i;
    }
  }
  return count;
}

typedef struct {
  size_t idx;
  double ratio;
} RatioItem;

static int cmp_ratio_item(const void *a, const void *b) {
  const RatioItem *x = a;
  const RatioItem *y = b;
  return (x->ratio < y->ratio) - (x->ratio > y->ratio); /* desc */
}

double fractional_knapsack(const double *weights, const double *values,
                           size_t n, double capacity, double *take) {
  for (size_t i = 0; i < n; i++) {
    take[i] = 0.0;
  }
  if (n == 0 || capacity <= 0.0) {
    return 0.0;
  }

  RatioItem *items = clrs_xmalloc(n * sizeof(RatioItem));
  for (size_t i = 0; i < n; i++) {
    CLRS_ASSERT(weights[i] > 0.0, "weight must be > 0");
    items[i].idx = i;
    items[i].ratio = values[i] / weights[i];
  }
  qsort(items, n, sizeof(RatioItem), cmp_ratio_item);

  double rem = capacity;
  double total = 0.0;
  for (size_t k = 0; k < n && rem > 0.0; k++) {
    size_t i = items[k].idx;
    if (weights[i] <= rem) {
      take[i] = 1.0;
      rem -= weights[i];
      total += values[i];
    } else {
      take[i] = rem / weights[i];
      total += values[i] * take[i];
      rem = 0.0;
    }
  }
  free(items);
  return total;
}

/* ---- Huffman ---- */

typedef struct HNode {
  int freq;
  int symbol; /* leaf symbol index, or -1 for internal */
  struct HNode *left;
  struct HNode *right;
} HNode;

static int cmp_hnode_ptr(const void *a, const void *b) {
  HNode *const *x = a;
  HNode *const *y = b;
  return ((*x)->freq > (*y)->freq) - ((*x)->freq < (*y)->freq);
}

static void free_htree(HNode *x) {
  if (x == NULL) {
    return;
  }
  free_htree(x->left);
  free_htree(x->right);
  free(x);
}

static void fill_depth(HNode *x, int depth, int *code_len, int *max_leaf) {
  if (x == NULL) {
    return;
  }
  if (x->left == NULL && x->right == NULL) {
    code_len[x->symbol] = depth;
    if (depth > *max_leaf) {
      *max_leaf = depth;
    }
    return;
  }
  fill_depth(x->left, depth + 1, code_len, max_leaf);
  fill_depth(x->right, depth + 1, code_len, max_leaf);
}

double huffman_wpl(const int *freq, size_t n, int *code_len_out) {
  CLRS_ASSERT(n > 0, "empty alphabet");
  if (n == 1) {
    if (code_len_out != NULL) {
      code_len_out[0] = 0;
    }
    return 0.0;
  }

  HNode **pool = clrs_xmalloc(n * sizeof(HNode *));
  for (size_t i = 0; i < n; i++) {
    CLRS_ASSERT(freq[i] > 0, "freq must be positive");
    pool[i] = clrs_xmalloc(sizeof(HNode));
    pool[i]->freq = freq[i];
    pool[i]->symbol = (int)i;
    pool[i]->left = pool[i]->right = NULL;
  }
  size_t count = n;

  while (count > 1) {
    qsort(pool, count, sizeof(HNode *), cmp_hnode_ptr);
    HNode *left = pool[0];
    HNode *right = pool[1];
    HNode *parent = clrs_xmalloc(sizeof(HNode));
    parent->freq = left->freq + right->freq;
    parent->symbol = -1;
    parent->left = left;
    parent->right = right;
    pool[0] = parent;
    pool[1] = pool[count - 1];
    count--;
  }

  HNode *root = pool[0];
  free(pool);

  int *depths = clrs_xcalloc(n, sizeof(int));
  int dummy_max = 0;
  fill_depth(root, 0, depths, &dummy_max);

  double wpl = 0.0;
  for (size_t i = 0; i < n; i++) {
    wpl += (double)freq[i] * (double)depths[i];
    if (code_len_out != NULL) {
      code_len_out[i] = depths[i];
    }
  }
  free(depths);
  free_htree(root);
  return wpl;
}
