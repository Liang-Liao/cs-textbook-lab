#ifndef MLAB_GRAPH_H
#define MLAB_GRAPH_H

/* D3 图算法：最短路 / 生成树 / 遍历 / 拓扑排序 */

/* 邻接矩阵：w[i*n+j]，不可达用 MLAB_GRAPH_INF */
#define MLAB_GRAPH_INF 1e300

typedef struct {
    int n;
    double *w; /* n*n 行主序；对角 0 */
} mlab_graph;

int mlab_graph_alloc(mlab_graph *g, int n);
void mlab_graph_free(mlab_graph *g);
/* 无向边：同时写 (i,j) 与 (j,i) */
void mlab_graph_set_edge(mlab_graph *g, int i, int j, double w);
void mlab_graph_set_directed(mlab_graph *g, int i, int j, double w);

/* 连通随机无向图（保证生成树骨架 + 额外边），权重 [wmin,wmax] */
int mlab_graph_random_connected(mlab_graph *g, int n, double p_extra,
                                double wmin, double wmax,
                                unsigned seed);

/* Dijkstra 单源：dist[n]，prev[n] 可空；不可达 = INF。
 * 检到负权边返回 -2（Dijkstra 不适用于负权，改用 Bellman-Ford）。 */
int mlab_graph_dijkstra(const mlab_graph *g, int src, double *dist, int *prev);

/* Floyd-Warshall 全源：dist n*n */
int mlab_graph_floyd(const mlab_graph *g, double *dist);

/* Bellman-Ford 单源，支持负权；has_neg_cycle 置 1 若检出负环 */
int mlab_graph_bellman_ford(const mlab_graph *g, int src, double *dist,
                            int *prev, int *has_neg_cycle);

/* BFS/DFS 访问序 */
int mlab_graph_bfs(const mlab_graph *g, int src, int *order, int *nvisited);
int mlab_graph_dfs(const mlab_graph *g, int src, int *order, int *nvisited);

/* 拓扑排序：有向无环；order 长 n；返回 0 成功，-1 有环 */
int mlab_graph_topo_sort(const mlab_graph *g, int *order);

/*
 * 最大流 Edmonds-Karp（BFS 增广路径；路线图 :647 选做）。
 * cap: n×n 容量矩阵（有向，行主序）；返回最大流值。
 * flow_out 可空，写入各边净流量（行主序，与 cap 同布局）。
 */
double mlab_graph_maxflow_ek(const double *cap, int n, int s, int t,
                             double *flow_out);

/* MST 权值：Prim / Kruskal（无向，INF 边忽略） */
double mlab_graph_mst_prim(const mlab_graph *g, int *parent);
double mlab_graph_mst_kruskal(const mlab_graph *g, int *parent);

#endif /* MLAB_GRAPH_H */
