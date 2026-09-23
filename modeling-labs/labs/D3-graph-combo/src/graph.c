#include "graph.h"
#include "rng.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
int mlab_graph_alloc(mlab_graph *g, int n)
{
    int i;
    if (!g || n <= 0) return -1;
    memset(g, 0, sizeof *g);
    g->n = n;
    g->w = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!g->w) return -1;
    for (i = 0; i < n * n; ++i) g->w[i] = MLAB_GRAPH_INF;
    for (i = 0; i < n; ++i) g->w[i * n + i] = 0.0;
    return 0;
}

void mlab_graph_free(mlab_graph *g)
{
    if (!g) return;
    free(g->w);
    g->w = NULL;
    g->n = 0;
}

void mlab_graph_set_edge(mlab_graph *g, int i, int j, double w)
{
    if (!g || i < 0 || j < 0 || i >= g->n || j >= g->n) return;
    g->w[i * g->n + j] = w;
    g->w[j * g->n + i] = w;
}

void mlab_graph_set_directed(mlab_graph *g, int i, int j, double w)
{
    if (!g || i < 0 || j < 0 || i >= g->n || j >= g->n) return;
    g->w[i * g->n + j] = w;
}

int mlab_graph_random_connected(mlab_graph *g, int n, double p_extra,
                                double wmin, double wmax, unsigned seed)
{
    mlab_rng rng;
    int i;
    if (mlab_graph_alloc(g, n) != 0) return -1;
    mlab_rng_seed(&rng, seed);
    /* 随机树骨架保证连通 */
    for (i = 1; i < n; ++i) {
        int j = (int)(mlab_rng_uniform(&rng) * (double)i);
        if (j >= i) j = i - 1;
        if (j < 0) j = 0;
        mlab_graph_set_edge(g, i, j,
                            wmin + (wmax - wmin) * mlab_rng_uniform(&rng));
    }
    /* 额外边 */
    for (i = 0; i < n; ++i) {
        int j;
        for (j = i + 1; j < n; ++j) {
            if (mlab_rng_uniform(&rng) < p_extra)
                mlab_graph_set_edge(g, i, j,
                                    wmin + (wmax - wmin) * mlab_rng_uniform(&rng));
        }
    }
    return 0;
}

int mlab_graph_dijkstra(const mlab_graph *g, int src, double *dist, int *prev)
{
    int n, i, u, v;
    char *done = NULL;
    if (!g || !dist || src < 0 || src >= g->n) return -1;
    n = g->n;
    /* 负权守卫：Dijkstra 贪心前提被破坏，结果不可信 → 显式拒算 */
    for (i = 0; i < n * n; ++i)
        if (g->w[i] < MLAB_GRAPH_INF * 0.5 && g->w[i] < -1e-12) return -2;
    done = (char *)calloc((size_t)n, 1);
    if (!done) return -1;
    for (i = 0; i < n; ++i) {
        dist[i] = MLAB_GRAPH_INF;
        if (prev) prev[i] = -1;
    }
    dist[src] = 0.0;
    for (i = 0; i < n; ++i) {
        double best = MLAB_GRAPH_INF;
        u = -1;
        for (v = 0; v < n; ++v) {
            if (!done[v] && dist[v] < best) {
                best = dist[v];
                u = v;
            }
        }
        if (u < 0 || best >= MLAB_GRAPH_INF * 0.5) break;
        done[u] = 1;
        for (v = 0; v < n; ++v) {
            double w = g->w[u * n + v];
            double nd;
            if (done[v] || w >= MLAB_GRAPH_INF * 0.5) continue;
            nd = dist[u] + w;
            if (nd < dist[v]) {
                dist[v] = nd;
                if (prev) prev[v] = u;
            }
        }
    }
    free(done);
    return 0;
}

int mlab_graph_floyd(const mlab_graph *g, double *dist)
{
    int n, i, j, k;
    if (!g || !dist) return -1;
    n = g->n;
    memcpy(dist, g->w, (size_t)n * (size_t)n * sizeof(double));
    for (k = 0; k < n; ++k)
        for (i = 0; i < n; ++i) {
            double dik = dist[i * n + k];
            if (dik >= MLAB_GRAPH_INF * 0.5) continue;
            for (j = 0; j < n; ++j) {
                double nd = dik + dist[k * n + j];
                if (nd < dist[i * n + j]) dist[i * n + j] = nd;
            }
        }
    return 0;
}

int mlab_graph_bellman_ford(const mlab_graph *g, int src, double *dist,
                            int *prev, int *has_neg_cycle)
{
    int n, i, u, v, iter;
    if (!g || !dist || src < 0 || src >= g->n) return -1;
    n = g->n;
    if (has_neg_cycle) *has_neg_cycle = 0;
    for (i = 0; i < n; ++i) {
        dist[i] = MLAB_GRAPH_INF;
        if (prev) prev[i] = -1;
    }
    dist[src] = 0.0;
    for (iter = 0; iter < n - 1; ++iter) {
        int updated = 0;
        for (u = 0; u < n; ++u) {
            if (dist[u] >= MLAB_GRAPH_INF * 0.5) continue;
            for (v = 0; v < n; ++v) {
                double w = g->w[u * n + v];
                double nd;
                if (w >= MLAB_GRAPH_INF * 0.5) continue;
                nd = dist[u] + w;
                if (nd < dist[v]) {
                    dist[v] = nd;
                    if (prev) prev[v] = u;
                    updated = 1;
                }
            }
        }
        if (!updated) break;
    }
    for (u = 0; u < n; ++u) {
        if (dist[u] >= MLAB_GRAPH_INF * 0.5) continue;
        for (v = 0; v < n; ++v) {
            double w = g->w[u * n + v];
            double tol;
            if (w >= MLAB_GRAPH_INF * 0.5) continue;
            /* 容差相对化：绝对 1e-12 对大量级 dist 会误报负环 */
            tol = 1e-9 * fmax(1.0, fabs(dist[v]));
            if (dist[u] + w < dist[v] - tol) {
                if (has_neg_cycle) *has_neg_cycle = 1;
                return 0;
            }
        }
    }
    return 0;
}

int mlab_graph_bfs(const mlab_graph *g, int src, int *order, int *nvisited)
{
    int n, head = 0, tail = 0, i;
    int *q;
    char *seen;
    if (!g || !order || src < 0 || src >= g->n) return -1;
    n = g->n;
    q = (int *)malloc((size_t)n * sizeof(int));
    seen = (char *)calloc((size_t)n, 1);
    if (!q || !seen) {
        free(q);
        free(seen);
        return -1;
    }
    q[tail++] = src;
    seen[src] = 1;
    while (head < tail) {
        int u = q[head++];
        order[head - 1] = u;
        for (i = 0; i < n; ++i) {
            if (!seen[i] && g->w[u * n + i] < MLAB_GRAPH_INF * 0.5 &&
                i != u) {
                seen[i] = 1;
                q[tail++] = i;
            }
        }
    }
    if (nvisited) *nvisited = tail;
    free(q);
    free(seen);
    return 0;
}

static void dfs_rec(const mlab_graph *g, int u, char *seen, int *order, int *cnt)
{
    int i;
    seen[u] = 1;
    order[(*cnt)++] = u;
    for (i = 0; i < g->n; ++i) {
        if (!seen[i] && g->w[u * g->n + i] < MLAB_GRAPH_INF * 0.5 && i != u)
            dfs_rec(g, i, seen, order, cnt);
    }
}

int mlab_graph_dfs(const mlab_graph *g, int src, int *order, int *nvisited)
{
    char *seen;
    int cnt = 0;
    if (!g || !order || src < 0 || src >= g->n) return -1;
    seen = (char *)calloc((size_t)g->n, 1);
    if (!seen) return -1;
    dfs_rec(g, src, seen, order, &cnt);
    if (nvisited) *nvisited = cnt;
    free(seen);
    return 0;
}

int mlab_graph_topo_sort(const mlab_graph *g, int *order)
{
    int n, i, j, written = 0;
    int *indeg;
    char *done;
    if (!g || !order) return -1;
    n = g->n;
    indeg = (int *)calloc((size_t)n, sizeof(int));
    done = (char *)calloc((size_t)n, 1);
    if (!indeg || !done) {
        free(indeg);
        free(done);
        return -1;
    }
    /* 约定：调用方用 set_directed 构造 DAG；对称无向边会互相入度 → 检出环 */
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            if (j == i) continue;
            if (g->w[j * n + i] < MLAB_GRAPH_INF * 0.5)
                indeg[i]++;
        }
    }
    while (written < n) {
        int u = -1;
        for (i = 0; i < n; ++i) {
            if (!done[i] && indeg[i] == 0) {
                u = i;
                break;
            }
        }
        if (u < 0) break; /* 有环 */
        order[written++] = u;
        done[u] = 1;
        for (i = 0; i < n; ++i) {
            if (!done[i] && g->w[u * n + i] < MLAB_GRAPH_INF * 0.5 && i != u)
                indeg[i]--;
        }
    }
    free(indeg);
    free(done);
    return (written == n) ? 0 : -1;
}

double mlab_graph_mst_prim(const mlab_graph *g, int *parent)
{
    int n, i, u, v;
    double *key;
    char *in;
    double total = 0.0;
    if (!g || g->n <= 0) return -1.0;
    n = g->n;
    key = (double *)malloc((size_t)n * sizeof(double));
    in = (char *)calloc((size_t)n, 1);
    if (!key || !in) {
        free(key);
        free(in);
        return -1.0;
    }
    for (i = 0; i < n; ++i) {
        key[i] = MLAB_GRAPH_INF;
        if (parent) parent[i] = -1;
    }
    key[0] = 0.0;
    for (u = 0; u < n; ++u) {
        double best = MLAB_GRAPH_INF;
        int p = -1;
        for (v = 0; v < n; ++v) {
            if (!in[v] && key[v] < best) {
                best = key[v];
                p = v;
            }
        }
        if (p < 0) break;
        in[p] = 1;
        total += key[p];
        for (v = 0; v < n; ++v) {
            double w = g->w[p * n + v];
            if (!in[v] && w < MLAB_GRAPH_INF * 0.5 && w < key[v]) {
                key[v] = w;
                if (parent) parent[v] = p;
            }
        }
    }
    free(key);
    free(in);
    return total;
}

typedef struct {
    int u, v;
    double w;
} mlab_edge;

static int edge_cmp(const void *a, const void *b)
{
    double da = ((const mlab_edge *)a)->w;
    double db = ((const mlab_edge *)b)->w;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static int uf_find(int *p, int x)
{
    while (p[x] != x) {
        p[x] = p[p[x]];
        x = p[x];
    }
    return x;
}

double mlab_graph_mst_kruskal(const mlab_graph *g, int *parent)
{
    int n, i, j, ne = 0, taken = 0;
    mlab_edge *edges;
    int *uf;
    double total = 0.0;
    if (!g || g->n <= 0) return -1.0;
    n = g->n;
    edges = (mlab_edge *)malloc((size_t)n * (size_t)n * sizeof(mlab_edge));
    uf = (int *)malloc((size_t)n * sizeof(int));
    if (!edges || !uf) {
        free(edges);
        free(uf);
        return -1.0;
    }
    for (i = 0; i < n; ++i) {
        uf[i] = i;
        if (parent) parent[i] = -1;
        for (j = i + 1; j < n; ++j) {
            double w = g->w[i * n + j];
            if (w < MLAB_GRAPH_INF * 0.5) {
                edges[ne].u = i;
                edges[ne].v = j;
                edges[ne].w = w;
                ++ne;
            }
        }
    }
    qsort(edges, (size_t)ne, sizeof(mlab_edge), edge_cmp);
    for (i = 0; i < ne && taken < n - 1; ++i) {
        int a = uf_find(uf, edges[i].u);
        int b = uf_find(uf, edges[i].v);
        if (a != b) {
            uf[a] = b;
            total += edges[i].w;
            if (parent) parent[edges[i].v] = edges[i].u;
            ++taken;
        }
    }
    free(edges);
    free(uf);
    return total;
}

/* ---------- 最大流 Edmonds-Karp（路线图 :647 选做） ---------- */

/* 残量网络上 BFS 找 s-t 路径；parent 记录前驱，返回 1 找到 */
static int ek_bfs(const double *res, int n, int s, int t, int *parent)
{
    int head = 0, tail = 0, i, u;
    char *seen = (char *)calloc((size_t)n, 1);
    int *q = (int *)malloc((size_t)n * sizeof(int));
    int found = 0;
    if (!seen || !q) {
        free(seen);
        free(q);
        return -1;
    }
    for (i = 0; i < n; ++i) parent[i] = -1;
    q[tail++] = s;
    seen[s] = 1;
    while (head < tail && !found) {
        u = q[head++];
        for (i = 0; i < n; ++i) {
            if (!seen[i] && res[u * n + i] > 1e-12) {
                seen[i] = 1;
                parent[i] = u;
                if (i == t) {
                    found = 1;
                    break;
                }
                q[tail++] = i;
            }
        }
    }
    free(seen);
    free(q);
    return found;
}

double mlab_graph_maxflow_ek(const double *cap, int n, int s, int t,
                             double *flow_out)
{
    double *res;
    double total = 0.0;
    int *parent;
    int i, j;
    if (!cap || n <= 0 || s < 0 || t < 0 || s >= n || t >= n || s == t)
        return -1.0;
    res = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    parent = (int *)malloc((size_t)n * sizeof(int));
    if (!res || !parent) {
        free(res);
        free(parent);
        return -1.0;
    }
    memcpy(res, cap, (size_t)n * (size_t)n * sizeof(double));
    if (flow_out)
        memset(flow_out, 0, (size_t)n * (size_t)n * sizeof(double));

    while (ek_bfs(res, n, s, t, parent) == 1) {
        double b = 1e300;
        int v = t;
        /* 瓶颈 */
        while (v != s) {
            int u = parent[v];
            if (res[u * n + v] < b) b = res[u * n + v];
            v = u;
        }
        /* 沿路更新残量 */
        v = t;
        while (v != s) {
            int u = parent[v];
            res[u * n + v] -= b;
            res[v * n + u] += b;
            v = u;
        }
        total += b;
    }
    if (flow_out) {
        for (i = 0; i < n; ++i)
            for (j = 0; j < n; ++j)
                flow_out[i * n + j] = cap[i * n + j] > res[i * n + j]
                                          ? cap[i * n + j] - res[i * n + j]
                                          : 0.0;
    }
    free(res);
    free(parent);
    return total;
}
