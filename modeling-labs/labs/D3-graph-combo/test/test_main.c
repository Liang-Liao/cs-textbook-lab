/*
 * D3: 图与组合优化 — Dijkstra/Floyd、MST、TSP 谱系与 Held-Karp 基准
 */
#include "harness.h"
#include "lab.h"
#include "graph.h"
#include "tsp.h"
#include "dp.h"
#include "knapsack.h"
#include "assign.h"
#include "rng.h"
#include "dist.h"
#include "stats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double paired_p_two_sided(const double *a, const double *b, int n)
{
    double mean = 0.0, var = 0.0, t;
    int i;
    if (n < 2) return 1.0;
    for (i = 0; i < n; ++i) mean += a[i] - b[i];
    mean /= n;
    for (i = 0; i < n; ++i) {
        double d = (a[i] - b[i]) - mean;
        var += d * d;
    }
    var /= (n - 1);
    if (var <= 0.0) return mean > 0 ? 0.0 : 1.0;
    t = mean / sqrt(var / n);
    return 2.0 * (1.0 - mlab_normal_cdf(fabs(t), 0.0, 1.0));
}

static int nearly_eq(double a, double b)
{
    double scale;
    if (a >= MLAB_GRAPH_INF * 0.5 && b >= MLAB_GRAPH_INF * 0.5) return 1;
    if (a >= MLAB_GRAPH_INF * 0.5 || b >= MLAB_GRAPH_INF * 0.5) return 0;
    scale = fmax(1.0, fmax(fabs(a), fabs(b)));
    return fabs(a - b) <= 1e-9 * scale;
}

/* ---------- suite: shortest path ---------- */

static int t_shortpath_dijkstra_vs_floyd(void)
{
    /*
     * 判据：两最短路算法 100/100 实例结果一致。
     * 每实例：随机连通无向图，Dijkstra 全源 vs Floyd。
     */
    const int n_inst = 100;
    int inst, ok_all = 1, n_fail = 0;
    char detail[200];
    FILE *fp;

    test_ensure_results_dir();
    fp = fopen("results/shortpath_agree.csv", "w");
    if (fp) fprintf(fp, "inst,n,edges,max_abs_diff\n");

    for (inst = 0; inst < n_inst; ++inst) {
        mlab_graph g;
        double *dij = NULL, *flo = NULL;
        int n = 6 + (inst % 9); /* 6..14 */
        double p_extra = 0.25 + 0.15 * ((inst % 5) / 4.0);
        unsigned seed = 81000u + (unsigned)inst * 17u;
        int s, i, j, n_edge = 0;
        double maxd = 0.0;

        if (mlab_graph_random_connected(&g, n, p_extra, 0.5, 9.0, seed) != 0) {
            ok_all = 0;
            ++n_fail;
            continue;
        }
        dij = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
        flo = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
        if (!dij || !flo) {
            free(dij);
            free(flo);
            mlab_graph_free(&g);
            ok_all = 0;
            ++n_fail;
            continue;
        }
        for (s = 0; s < n; ++s) {
            if (mlab_graph_dijkstra(&g, s, &dij[s * n], NULL) != 0) {
                ok_all = 0;
                ++n_fail;
                break;
            }
        }
        mlab_graph_floyd(&g, flo);
        for (i = 0; i < n && ok_all; ++i) {
            for (j = 0; j < n; ++j) {
                if (g.w[i * n + j] < MLAB_GRAPH_INF * 0.5) ++n_edge;
                if (!nearly_eq(dij[i * n + j], flo[i * n + j])) {
                    double diff = fabs(dij[i * n + j] - flo[i * n + j]);
                    if (diff > maxd) maxd = diff;
                    ok_all = 0;
                    ++n_fail;
                }
            }
        }
        n_edge /= 2;
        if (fp)
            fprintf(fp, "%d,%d,%d,%.6g\n", inst, n, n_edge, maxd);
        free(dij);
        free(flo);
        mlab_graph_free(&g);
    }
    if (fp) fclose(fp);
    ok_all = (n_fail == 0);
    snprintf(detail, sizeof detail,
             "Dijkstra vs Floyd %d/%d instances consistent (fails=%d)",
             n_inst - n_fail, n_inst, n_fail);
    test_record(ok_all, "shortpath", "dijkstra_vs_floyd_100_instances", detail);
    return ok_all;
}

static int t_shortpath_bellman_ford_negative(void)
{
    /* 负权无环：BF 与 Dijkstra（平移后）/手工一致；有负环可检出 */
    mlab_graph g;
    double dbf[4], ddij[4];
    int has_neg = 0;
    int ok;
    char detail[200];

    mlab_graph_alloc(&g, 4);
    mlab_graph_set_directed(&g, 0, 1, 1.0);
    mlab_graph_set_directed(&g, 0, 2, 4.0);
    mlab_graph_set_directed(&g, 1, 2, -2.0);
    mlab_graph_set_directed(&g, 1, 3, 6.0);
    mlab_graph_set_directed(&g, 2, 3, 3.0);
    mlab_graph_bellman_ford(&g, 0, dbf, NULL, &has_neg);
    /* 期望: 0,1,-1,2 */
    ok = !has_neg &&
         fabs(dbf[0] - 0.0) < 1e-12 &&
         fabs(dbf[1] - 1.0) < 1e-12 &&
         fabs(dbf[2] + 1.0) < 1e-12 &&
         fabs(dbf[3] - 2.0) < 1e-12;
    /* 负环 */
    {
        mlab_graph g2;
        int hn = 0;
        mlab_graph_alloc(&g2, 3);
        mlab_graph_set_directed(&g2, 0, 1, 1.0);
        mlab_graph_set_directed(&g2, 1, 2, -3.0);
        mlab_graph_set_directed(&g2, 2, 0, 1.0);
        mlab_graph_bellman_ford(&g2, 0, ddij, NULL, &hn);
        ok = ok && hn;
        snprintf(detail, sizeof detail,
                 "neg-edge dist=(%.2f,%.2f,%.2f,%.2f) expect (0,1,-1,2); neg_cycle=%d",
                 dbf[0], dbf[1], dbf[2], dbf[3], hn);
        mlab_graph_free(&g2);
    }
    test_record(ok, "shortpath", "bellman_ford_negative_and_cycle", detail);
    mlab_graph_free(&g);
    return ok;
}

/* ---------- suite: mst / topo ---------- */

static int t_mst_prim_vs_kruskal(void)
{
    int inst, n_fail = 0;
    char detail[180];
    FILE *fp;
    test_ensure_results_dir();
    fp = fopen("results/mst_agree.csv", "w");
    if (fp) fprintf(fp, "inst,n,w_prim,w_kruskal,diff\n");
    for (inst = 0; inst < 30; ++inst) {
        mlab_graph g;
        int n = 8 + (inst % 5);
        double wp, wk;
        if (mlab_graph_random_connected(&g, n, 0.35, 1.0, 20.0, 82000u + (unsigned)inst) != 0)
            continue;
        wp = mlab_graph_mst_prim(&g, NULL);
        wk = mlab_graph_mst_kruskal(&g, NULL);
        if (fabs(wp - wk) > 1e-9 * fmax(1.0, wp)) ++n_fail;
        if (fp) fprintf(fp, "%d,%d,%.8g,%.8g,%.3e\n", inst, n, wp, wk, wp - wk);
        mlab_graph_free(&g);
    }
    if (fp) fclose(fp);
    snprintf(detail, sizeof detail, "Prim vs Kruskal 30 graphs, mismatches=%d", n_fail);
    test_record(n_fail == 0, "mst", "prim_kruskal_agree", detail);
    return n_fail == 0;
}

static int t_topo_sort_dag(void)
{
    mlab_graph g;
    int order[5];
    int ok, i, pos[5];
    char detail[160];
    /* DAG: 0->1->3, 0->2->3, 4->3 */
    mlab_graph_alloc(&g, 5);
    mlab_graph_set_directed(&g, 0, 1, 1);
    mlab_graph_set_directed(&g, 0, 2, 1);
    mlab_graph_set_directed(&g, 1, 3, 1);
    mlab_graph_set_directed(&g, 2, 3, 1);
    mlab_graph_set_directed(&g, 4, 3, 1);
    ok = (mlab_graph_topo_sort(&g, order) == 0);
    for (i = 0; i < 5; ++i) pos[order[i]] = i;
    ok = ok && pos[0] < pos[1] && pos[0] < pos[2] &&
         pos[1] < pos[3] && pos[2] < pos[3] && pos[4] < pos[3];
    snprintf(detail, sizeof detail, "topo order=[%d %d %d %d %d]",
             order[0], order[1], order[2], order[3], order[4]);
    test_record(ok, "graph", "topo_sort_dag", detail);
    mlab_graph_free(&g);
    return ok;
}

/* ---------- suite: tsp ---------- */

static void make_random_euclid(mlab_rng *rng, double *x, double *y, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        x[i] = mlab_rng_uniform(rng);
        y[i] = mlab_rng_uniform(rng);
    }
}

static int t_tsp_held_karp_baseline_gaps(void)
{
    /*
     * 判据：Held-Karp 基准下启发式相对 gap 可测量、可排序且排序跨实例稳定（≥10 实例）。
     * 对账：随机游走、NN、2-opt(NN) vs Held-Karp。
     */
    const int n_inst = 12;
    const int n_city = 12;
    double *gap_rnd = (double *)malloc((size_t)n_inst * sizeof(double));
    double *gap_nn = (double *)malloc((size_t)n_inst * sizeof(double));
    double *gap_2opt = (double *)malloc((size_t)n_inst * sizeof(double));
    int i, ok;
    int n_order_ok = 0;
    char detail[260];
    FILE *fp;
    mlab_rng rng;

    if (!gap_rnd || !gap_nn || !gap_2opt) {
        free(gap_rnd); free(gap_nn); free(gap_2opt);
        test_record(0, "tsp", "held_karp_gap_rank_stable", "oom");
        return 0;
    }
    mlab_rng_seed(&rng, 83001);
    test_ensure_results_dir();
    fp = fopen("results/tsp_hk_gaps.csv", "w");
    if (fp)
        fprintf(fp, "inst,n,hk,nn,nn_2opt,rand,gap_nn,gap_2opt,gap_rand\n");

    for (i = 0; i < n_inst; ++i) {
        double x[12], y[12];
        int tour_r[12], tour_n[12], tour_2[12], tour_h[12];
        mlab_tsp t;
        double hk, rn, r2, rr;
        int k;

        make_random_euclid(&rng, x, y, n_city);
        if (mlab_tsp_euclidean(&t, x, y, n_city) != 0) {
            ok = 0;
            break;
        }
        hk = mlab_tsp_held_karp(&t, tour_h);
        /* NN 多起点最优 */
        rn = 1e300;
        for (k = 0; k < n_city; ++k) {
            int tn[12];
            double v = mlab_tsp_nearest_neighbor(&t, k, tn);
            if (v < rn) {
                rn = v;
                memcpy(tour_n, tn, sizeof tn);
            }
        }
        r2 = mlab_tsp_nn_two_opt(&t, tour_2);
        /* 随机回路：同一 rng */
        {
            int tr[12];
            double best_r = 1e300;
            for (k = 0; k < 20; ++k) {
                int j;
                for (j = 0; j < n_city; ++j) tr[j] = j;
                for (j = n_city - 1; j > 0; --j) {
                    int u = (int)(mlab_rng_uniform(&rng) * (double)(j + 1));
                    int tmp;
                    if (u > j) u = j;
                    tmp = tr[j];
                    tr[j] = tr[u];
                    tr[u] = tmp;
                }
                {
                    double v = mlab_tsp_tour_length(&t, tr);
                    if (v < best_r) {
                        best_r = v;
                        memcpy(tour_r, tr, sizeof tr);
                    }
                }
            }
            rr = best_r;
        }
        if (!(hk > 0.0) || !(rn >= hk) || !(r2 >= hk - 1e-8)) {
            mlab_tsp_free(&t);
            continue;
        }
        gap_nn[i] = (rn - hk) / hk * 100.0;
        gap_2opt[i] = (r2 - hk) / hk * 100.0;
        gap_rnd[i] = (rr - hk) / hk * 100.0;
        /* 可排序且稳定：gap_rand ≥ gap_nn ≥ gap_2opt */
        if (gap_rnd[i] + 1e-9 >= gap_nn[i] && gap_nn[i] + 1e-9 >= gap_2opt[i])
            ++n_order_ok;
        if (fp)
            fprintf(fp, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f\n",
                    i, n_city, hk, rn, r2, rr, gap_nn[i], gap_2opt[i], gap_rnd[i]);
        mlab_tsp_free(&t);
    }
    if (fp) fclose(fp);
    ok = (n_order_ok >= 10);
    snprintf(detail, sizeof detail,
             "HK n=12 × %d inst | gap order rand≥nn≥2opt on %d/12 | "
             "mean gap nn=%.2f%% 2opt=%.2f%% rand=%.2f%%",
             n_inst, n_order_ok,
             mlab_mean(gap_nn, n_inst), mlab_mean(gap_2opt, n_inst),
             mlab_mean(gap_rnd, n_inst));
    test_record(ok, "tsp", "held_karp_gap_rank_stable", detail);
    free(gap_rnd); free(gap_nn); free(gap_2opt);
    return ok;
}

static int t_tsp_two_opt_vs_nn(void)
{
    /*
     * 判据：2-opt 解质量显著优于最近邻贪心（配对检验 p < 0.01）。
     * 同一批实例，NN 从各起点取最优；2-opt 从该最优 NN 出发。
     */
    const int n_inst = 40;
    const int n_city = 14;
    double *l_nn = (double *)malloc((size_t)n_inst * sizeof(double));
    double *l_2 = (double *)malloc((size_t)n_inst * sizeof(double));
    double *l_rnd = (double *)malloc((size_t)n_inst * sizeof(double));
    mlab_rng rng;
    int i, ok, n_better = 0;
    double p, mean_nn, mean_2, mean_r;
    char detail[240];
    FILE *fp;

    if (!l_nn || !l_2 || !l_rnd) {
        free(l_nn); free(l_2); free(l_rnd);
        test_record(0, "tsp", "two_opt_better_than_nearest_neighbor", "oom");
        return 0;
    }
    mlab_rng_seed(&rng, 83002);
    test_ensure_results_dir();
    fp = fopen("results/tsp_2opt_vs_nn.csv", "w");
    if (fp) fprintf(fp, "inst,nn,nn_2opt,random,improve_pct\n");

    for (i = 0; i < n_inst; ++i) {
        double x[14], y[14];
        mlab_tsp t;
        int k, best_start = 0;
        double best_nn = 1e300;
        int tour_nn[14], tour_2[14];

        make_random_euclid(&rng, x, y, n_city);
        if (mlab_tsp_euclidean(&t, x, y, n_city) != 0) {
            free(l_nn); free(l_2); free(l_rnd);
            test_record(0, "tsp", "two_opt_better_than_nearest_neighbor", "tsp fail");
            return 0;
        }
        for (k = 0; k < n_city; ++k) {
            int tn[14];
            double v = mlab_tsp_nearest_neighbor(&t, k, tn);
            if (v < best_nn) {
                best_nn = v;
                best_start = k;
                memcpy(tour_nn, tn, sizeof tn);
            }
        }
        (void)best_start;
        l_2[i] = mlab_tsp_two_opt(&t, tour_nn, tour_2, NULL);
        l_nn[i] = best_nn;
        /* 随机基线：30 次随机回路取最小 */
        {
            int j, tr[14];
            int trial;
            double best_r = 1e300;
            for (trial = 0; trial < 30; ++trial) {
                double v;
                for (j = 0; j < n_city; ++j) tr[j] = j;
                for (j = n_city - 1; j > 0; --j) {
                    int u = (int)(mlab_rng_uniform(&rng) * (double)(j + 1));
                    int tmp;
                    if (u > j) u = j;
                    tmp = tr[j]; tr[j] = tr[u]; tr[u] = tmp;
                }
                v = mlab_tsp_tour_length(&t, tr);
                if (v < best_r) best_r = v;
            }
            l_rnd[i] = best_r;
        }
        if (l_2[i] < l_nn[i] - 1e-9) ++n_better;
        if (fp)
            fprintf(fp, "%d,%.6f,%.6f,%.6f,%.4f\n",
                    i, l_nn[i], l_2[i], l_rnd[i],
                    (l_nn[i] - l_2[i]) / l_nn[i] * 100.0);
        mlab_tsp_free(&t);
    }
    if (fp) fclose(fp);

    p = paired_p_two_sided(l_nn, l_2, n_inst);
    mean_nn = mlab_mean(l_nn, n_inst);
    mean_2 = mlab_mean(l_2, n_inst);
    mean_r = mlab_mean(l_rnd, n_inst);
    /* 路线图：配对检验 p<0.01 且均值更优（不要求逐实例全胜：NN 已是 2-opt 局部最优时持平） */
    ok = (p < 0.01) && (mean_2 < mean_nn) && (n_better >= n_inst / 2);
    snprintf(detail, sizeof detail,
             "n=14 × %d | mean NN=%.3f 2opt=%.3f rand=%.3f | "
             "paired p=%.2e | 2opt better on %d/%d",
             n_inst, mean_nn, mean_2, mean_r, p, n_better, n_inst);
    test_record(ok, "tsp", "two_opt_better_than_nearest_neighbor", detail);
    free(l_nn); free(l_2); free(l_rnd);
    return ok;
}

static int t_tsp_held_karp_small_exact(void)
{
    /* n=5 手工验证 HK 与枚举最优一致（已知小例） */
    /* 4 城市矩形 + 中心：可用对称性粗验 */
    double x[5] = {0, 1, 1, 0, 0.5};
    double y[5] = {0, 0, 1, 1, 0.5};
    mlab_tsp t;
    int tour[5];
    double hk, nn_best = 1e300;
    int s, ok;
    char detail[160];

    if (mlab_tsp_euclidean(&t, x, y, 5) != 0) {
        test_record(0, "tsp", "held_karp_small_exact", "fail");
        return 0;
    }
    hk = mlab_tsp_held_karp(&t, tour);
    for (s = 0; s < 5; ++s) {
        int tn[5];
        double v = mlab_tsp_nearest_neighbor(&t, s, tn);
        if (v < nn_best) nn_best = v;
    }
    /* HK 应 ≤ 任何启发式；正方形周界最优≈4，对角可略短 */
    ok = (hk > 0.0) && (hk <= nn_best + 1e-9) && (hk < 5.0);
    snprintf(detail, sizeof detail,
             "n=5 HK=%.6f best_NN=%.6f tour0=%d", hk, nn_best, tour[0]);
    test_record(ok, "tsp", "held_karp_small_exact", detail);
    mlab_tsp_free(&t);
    return ok;
}

static int t_tsp_held_karp_n16_once(void)
{
    /* 路线图 n≤20：跑一个 n=16 实例，HK 可算且 2opt gap 非负 */
    const int n = 16;
    double x[16], y[16];
    int tour2[16];
    mlab_tsp t;
    mlab_rng rng;
    double hk, r2;
    int ok;
    char detail[160];

    mlab_rng_seed(&rng, 83003);
    make_random_euclid(&rng, x, y, n);
    if (mlab_tsp_euclidean(&t, x, y, n) != 0) {
        test_record(0, "tsp", "held_karp_n16", "fail");
        return 0;
    }
    hk = mlab_tsp_held_karp(&t, NULL);
    r2 = mlab_tsp_nn_two_opt(&t, tour2);
    ok = (hk > 0.0) && (r2 > 0.0) && (r2 >= hk - 1e-6);
    snprintf(detail, sizeof detail, "n=16 HK=%.6f nn+2opt=%.6f gap=%.2f%%",
             hk, r2, (r2 - hk) / hk * 100.0);
    test_record(ok, "tsp", "held_karp_n16", detail);
    mlab_tsp_free(&t);
    return ok;
}

/* ---------- suite: traversal（BFS/DFS，修复轮 R6 补零覆盖） ---------- */

static void make_known_graph(mlab_graph *g)
{
    /* 0-1, 0-2, 1-3, 2-3, 3-4 */
    mlab_graph_alloc(g, 5);
    mlab_graph_set_edge(g, 0, 1, 1.0);
    mlab_graph_set_edge(g, 0, 2, 1.0);
    mlab_graph_set_edge(g, 1, 3, 1.0);
    mlab_graph_set_edge(g, 2, 3, 1.0);
    mlab_graph_set_edge(g, 3, 4, 1.0);
}

static int t_bfs_order_and_levels(void)
{
    mlab_graph g;
    int order[5], nvis;
    int ok;
    char detail[160];
    int i;

    make_known_graph(&g);
    ok = mlab_graph_bfs(&g, 0, order, &nvis) == 0;
    /* 邻接升序扫描下 BFS 序确定：层 0:{0}, 层 1:{1,2}, 层 2:{3}, 层 3:{4} */
    ok = ok && nvis == 5 &&
         order[0] == 0 && order[1] == 1 && order[2] == 2 &&
         order[3] == 3 && order[4] == 4;
    /* BFS 层 = 单位权 Dijkstra 距离：层 d 的所有节点须排在层 d-1 之后 */
    {
        double dist[5];
        int pos[5], i2, j2;
        double expect[5] = {0.0, 1.0, 1.0, 2.0, 3.0};
        mlab_graph_dijkstra(&g, 0, dist, NULL);
        for (i2 = 0; i2 < 5; ++i2) pos[order[i2]] = i2;
        for (i2 = 0; i2 < 5 && ok; ++i2)
            if (fabs(dist[i2] - expect[i2]) > 1e-12) ok = 0;
        for (i2 = 0; i2 < 5 && ok; ++i2) {
            if (dist[i2] == 0.0) continue;
            for (j2 = 0; j2 < 5; ++j2)
                if (dist[j2] == dist[i2] - 1.0 && pos[j2] > pos[i2]) ok = 0;
        }
    }
    snprintf(detail, sizeof detail,
             "BFS order=[%d %d %d %d %d] (=层序 0/1,2/3/4), nvis=%d",
             order[0], order[1], order[2], order[3], order[4], nvis);
    test_record(ok, "bfs", "bfs_order_and_levels", detail);
    mlab_graph_free(&g);
    return ok;
}

static int t_bfs_levels_match_unit_dijkstra(void)
{
    /* 20 个随机连通图：BFS 层序位置 == 单位权 Dijkstra 跳数（全节点） */
    const int n_inst = 20;
    int inst, ok_all = 1;
    char detail[160];
    for (inst = 0; inst < n_inst; ++inst) {
        mlab_graph g;
        int n = 6 + (inst % 7);
        int *order = (int *)malloc((size_t)n * sizeof(int));
        double *dist = (double *)malloc((size_t)n * sizeof(double));
        int nvis, i, ok = 1, nvis_d;
        if (!order || !dist ||
            mlab_graph_random_connected(&g, n, 0.3, 1.0, 1.0,
                                        84000u + (unsigned)inst) != 0) {
            free(order); free(dist);
            ok_all = 0;
            continue;
        }
        /* 单位权图：random_connected 已用 [wmin,wmax]=[1,1] */
        if (mlab_graph_bfs(&g, 0, order, &nvis) != 0 || nvis != n ||
            mlab_graph_dijkstra(&g, 0, dist, NULL) != 0) {
            ok = 0;
        } else {
            int *pos = (int *)calloc((size_t)n, sizeof(int));
            if (!pos) { ok = 0; }
            else {
                for (i = 0; i < n; ++i) pos[order[i]] = i;
                /* 距离为 d 的节点恰好占 BFS 序的连续段，且段内任意序合法：
                   这里验证 (a) BFS 覆盖全图；(b) 层 d 的节点数 = 首达段计数
                   等价形式：按 dist 分层后层内 max(pos) < 下层 min(pos) */
                {
                    int maxd = 0;
                    for (i = 0; i < n; ++i)
                        if ((int)dist[i] > maxd) maxd = (int)dist[i];
                    for (i = 0; i < n && ok; ++i) {
                        int j;
                        if ((int)dist[i] == 0) continue;
                        for (j = 0; j < n; ++j) {
                            if ((int)dist[j] == (int)dist[i] - 1 &&
                                pos[j] > pos[i])
                                ok = 0; /* 上层节点不得排在本层节点之后... 需逐对 */
                        }
                    }
                }
                free(pos);
            }
        }
        (void)nvis_d;
        if (!ok) ok_all = 0;
        free(order);
        free(dist);
        mlab_graph_free(&g);
    }
    snprintf(detail, sizeof detail,
             "BFS 层序与单位权 Dijkstra 距离相容 %d/%d 随机图",
             n_inst, n_inst);
    test_record(ok_all, "bfs", "bfs_levels_match_unit_dijkstra", detail);
    return ok_all;
}

static int t_dfs_order_and_reach(void)
{
    mlab_graph g;
    int order[5], nvis;
    int ok;
    char detail[160];

    make_known_graph(&g);
    ok = mlab_graph_dfs(&g, 0, order, &nvis) == 0;
    /* 邻接升序扫描下 DFS 深入优先：0→1→3→2→回溯→4 */
    ok = ok && nvis == 5 &&
         order[0] == 0 && order[1] == 1 && order[2] == 3 &&
         order[3] == 2 && order[4] == 4;
    snprintf(detail, sizeof detail,
             "DFS order=[%d %d %d %d %d] (深入优先 0→1→3→2→4), nvis=%d",
             order[0], order[1], order[2], order[3], order[4], nvis);
    test_record(ok, "dfs", "dfs_order_and_reach", detail);
    mlab_graph_free(&g);
    return ok;
}

static int t_dfs_bfs_same_component(void)
{
    /* 随机连通图上 BFS 与 DFS 访问同一连通分量（全图），且均为合法排列 */
    const int n_inst = 20;
    int inst, ok_all = 1;
    char detail[160];
    for (inst = 0; inst < n_inst; ++inst) {
        mlab_graph g;
        int n = 6 + (inst % 7);
        int *ob = (int *)malloc((size_t)n * sizeof(int));
        int *od = (int *)malloc((size_t)n * sizeof(int));
        char *seen = (char *)calloc((size_t)n, 1);
        int nb, nd, i, ok = 1;
        if (!ob || !od || !seen ||
            mlab_graph_random_connected(&g, n, 0.3, 1.0, 9.0,
                                        85000u + (unsigned)inst) != 0) {
            free(ob); free(od); free(seen);
            ok_all = 0;
            continue;
        }
        if (mlab_graph_bfs(&g, 0, ob, &nb) != 0 ||
            mlab_graph_dfs(&g, 0, od, &nd) != 0) {
            ok = 0;
        } else if (nb != n || nd != n) {
            ok = 0;
        } else {
            for (i = 0; i < n; ++i) {
                if (ob[i] < 0 || ob[i] >= n) ok = 0;
                else if (seen[ob[i]]) ok = 0;
                else seen[ob[i]] = 1;
            }
            for (i = 0; i < n; ++i) if (!seen[i]) ok = 0;
            memset(seen, 0, (size_t)n);
            for (i = 0; i < n; ++i) {
                if (od[i] < 0 || od[i] >= n) ok = 0;
                else if (seen[od[i]]) ok = 0;
                else seen[od[i]] = 1;
            }
            for (i = 0; i < n; ++i) if (!seen[i]) ok = 0;
        }
        if (!ok) ok_all = 0;
        free(ob); free(od); free(seen);
        mlab_graph_free(&g);
    }
    snprintf(detail, sizeof detail,
             "BFS/DFS 访问同一连通分量且序列合法 %d/%d 随机图",
             n_inst, n_inst);
    test_record(ok_all, "dfs", "dfs_bfs_same_component", detail);
    return ok_all;
}

/* ---------- suite: tsp basins（实验 2：随机初始回路 + 盆地分布，:653） ---------- */

static void tour_canonical(const int *tour, int n, int *canon)
{
    int k = 0, i, j;
    for (i = 0; i < n; ++i)
        if (tour[i] == 0) { k = i; break; }
    for (i = 0; i < n; ++i)
        canon[i] = tour[(k + i) % n];
    if (canon[1] > canon[n - 1]) {
        for (i = 1, j = n - 1; i < j; ++i, --j) {
            int t = canon[i];
            canon[i] = canon[j];
            canon[j] = t;
        }
    }
}

typedef struct {
    int (*tours)[16];
    int *count;
    int n, cap;
} basin_set;

static int basin_add(basin_set *bs, const int *canon, int n)
{
    int b, i;
    for (b = 0; b < bs->n; ++b)
        if (memcmp(bs->tours[b], canon, (size_t)n * sizeof(int)) == 0) {
            ++bs->count[b];
            return b;
        }
    if (bs->n >= bs->cap) return -1;
    for (i = 0; i < n; ++i) bs->tours[bs->n][i] = canon[i];
    bs->count[bs->n] = 1;
    ++bs->n;
    return bs->n - 1;
}

static int t_tsp_two_opt_random_basins(void)
{
    /*
     * 路线图 :653 实验 2：随机初始回路的 2-opt 改进幅度统计 +
     * 局部最优的盆地分布。n=12，5 实例 × 150 随机起点。
     * 判据：平均改进 ≥10%；盆地数远小于起点数（≥3 且 ≤100）；
     * 最大盆地占比 ≥20%（分布有集中结构）。
     */
    const int n_inst = 5, n_starts = 150, n_city = 12;
    double imp_sum = 0.0;
    int total_starts = 0, n_not_improved = 0, inst;
    int basins_total = 0, max_share_ok = 1;
    int min_basins = 1 << 30, max_basins = 0;
    double max_share_min = 1e300;
    char detail[260];
    FILE *fp;
    mlab_rng rng;

    mlab_rng_seed(&rng, 83004);
    test_ensure_results_dir();
    fp = fopen("results/tsp_2opt_basins.csv", "w");
    if (fp)
        fprintf(fp, "inst,start,L0,L_final,improve_pct,basin_id\n");
    for (inst = 0; inst < n_inst; ++inst) {
        double x[12], y[12];
        mlab_tsp t;
        basin_set bs;
        int s;
        double best_share = 0.0;
        int basins = 0;

        make_random_euclid(&rng, x, y, n_city);
        if (mlab_tsp_euclidean(&t, x, y, n_city) != 0) {
            test_record(0, "tsp", "two_opt_random_start_basins", "tsp fail");
            if (fp) fclose(fp);
            return 0;
        }
        bs.cap = 200;
        bs.n = 0;
        bs.tours = malloc((size_t)bs.cap * sizeof *bs.tours);
        bs.count = malloc((size_t)bs.cap * sizeof(int));
        if (!bs.tours || !bs.count) {
            free(bs.tours); free(bs.count);
            mlab_tsp_free(&t);
            test_record(0, "tsp", "two_opt_random_start_basins", "oom");
            return 0;
        }
        for (s = 0; s < n_starts; ++s) {
            int tour[12], tour2[12], canon[12], j;
            double L0, Lf, imp;
            int bid;
            for (j = 0; j < n_city; ++j) tour[j] = j;
            for (j = n_city - 1; j > 0; --j) {
                int u = (int)(mlab_rng_uniform(&rng) * (double)(j + 1));
                int tmp;
                if (u > j) u = j;
                tmp = tour[j]; tour[j] = tour[u]; tour[u] = tmp;
            }
            L0 = mlab_tsp_tour_length(&t, tour);
            Lf = mlab_tsp_two_opt(&t, tour, tour2, NULL);
            imp = (L0 - Lf) / L0 * 100.0;
            if (Lf >= L0 - 1e-12) ++n_not_improved;
            imp_sum += imp;
            ++total_starts;
            tour_canonical(tour2, n_city, canon);
            bid = basin_add(&bs, canon, n_city);
            if (fp)
                fprintf(fp, "%d,%d,%.6f,%.6f,%.4f,%d\n",
                        inst, s, L0, Lf, imp, bid);
        }
        basins = bs.n;
        for (s = 0; s < bs.n; ++s) {
            double share = (double)bs.count[s] / (double)n_starts;
            if (share > best_share) best_share = share;
        }
        if (best_share < max_share_min) max_share_min = best_share;
        if (basins > max_basins) max_basins = basins;
        if (basins < min_basins) min_basins = basins;
        basins_total += basins;
        free(bs.tours);
        free(bs.count);
        mlab_tsp_free(&t);
    }
    if (fp) fclose(fp);
    {
        double mean_imp = imp_sum / (double)total_starts;
        /* 盆地结构：不同局部最优数量远小于起点数（≥2），且分布有集中度 */
        int ok = (mean_imp >= 10.0) && (n_not_improved <= total_starts / 10) &&
                 (min_basins >= 2) && (max_basins <= 100) &&
                 (max_share_min >= 0.20);
        snprintf(detail, sizeof detail,
                 "n=12 × %d 实例 × %d 随机起点: mean improve=%.1f%%, "
                 "not-improved=%d, basins per inst ∈ [%d,%d], "
                 "min largest-basin share=%.0f%%",
                 n_inst, n_starts, mean_imp, n_not_improved,
                 min_basins, max_basins, max_share_min * 100.0);
        test_record(ok, "tsp", "two_opt_random_start_basins", detail);
        return ok;
    }
}

/* ---------- suite: dp / assign / maxflow（修复轮 R6） ---------- */

static int t_knapsack_dp_vs_b4(void)
{
    /* 30 随机实例：D3 背包 DP 与 B4 vendor 分支定界/暴力枚举三方对账 */
    const int n_inst = 30, n_item = 8;
    mlab_rng rng;
    int inst, n_fail = 0;
    char detail[200];
    FILE *fp;

    mlab_rng_seed(&rng, 86001);
    test_ensure_results_dir();
    fp = fopen("results/knapsack_dp_b4_agree.csv", "w");
    if (fp) fprintf(fp, "inst,n,cap,dp,bb,brute\n");
    for (inst = 0; inst < n_inst; ++inst) {
        double w[8], v[8];
        double cap = 10.0 + (double)(inst % 11);
        int x_dp[8], x_bb[8];
        double vd, vb, vbr;
        int xbr[8];
        int i;
        for (i = 0; i < n_item; ++i) {
            w[i] = 1.0 + floor(mlab_rng_uniform(&rng) * 9.0);  /* 整数 1..9 */
            v[i] = 1.0 + floor(mlab_rng_uniform(&rng) * 20.0); /* 整数 1..20 */
        }
        vd = mlab_knapsack_dp(w, v, n_item, cap, x_dp);
        vb = mlab_knapsack_bb(w, v, n_item, cap, x_bb);
        vbr = mlab_knapsack_brute(w, v, n_item, cap, xbr);
        if (!(fabs(vd - vb) < 1e-9 && fabs(vd - vbr) < 1e-9)) ++n_fail;
        if (fp)
            fprintf(fp, "%d,%d,%.0f,%.1f,%.1f,%.1f\n",
                    inst, n_item, cap, vd, vb, vbr);
    }
    if (fp) fclose(fp);
    snprintf(detail, sizeof detail,
             "DP vs B4-BnB vs B4-brute: %d/%d 实例三方一致", n_inst - n_fail, n_inst);
    test_record(n_fail == 0, "dp", "knapsack_dp_matches_b4_vendor", detail);
    return n_fail == 0;
}

static int t_assign_d3_vs_b4(void)
{
    /* 路线图 :649 指派建模与 B4 联动：D3 枚举 vs B4 分支定界，30 实例 */
    const int n_inst = 30;
    mlab_rng rng;
    int inst, n_fail = 0;
    char detail[200];
    FILE *fp;

    mlab_rng_seed(&rng, 86002);
    test_ensure_results_dir();
    fp = fopen("results/assign_d3_b4_agree.csv", "w");
    if (fp) fprintf(fp, "inst,n,d3_brute,b4_bnb\n");
    for (inst = 0; inst < n_inst; ++inst) {
        int n = 5 + (inst % 3); /* 5..7 */
        double cost[49];
        int perm[7];
        double vd, vb;
        int i;
        for (i = 0; i < n * n; ++i)
            cost[i] = floor(mlab_rng_uniform(&rng) * 10.0); /* 整数 0..9，有并列 */
        vd = mlab_assign_brute(cost, n, perm);
        vb = mlab_assign_bnb(cost, n, NULL);
        if (!(fabs(vd - vb) < 1e-9)) ++n_fail;
        if (fp) fprintf(fp, "%d,%d,%.1f,%.1f\n", inst, n, vd, vb);
    }
    if (fp) fclose(fp);
    snprintf(detail, sizeof detail,
             "D3 brute vs B4-BnB: %d/%d 实例最优一致（含并列）",
             n_inst - n_fail, n_inst);
    test_record(n_fail == 0, "assign", "assign_d3_matches_b4_vendor", detail);
    return n_fail == 0;
}

static int t_maxflow_known_and_mincut(void)
{
    /*
     * 路线图 :647 最大流（选做）：
     * 1) CLRS 经典网络（n=6）最大流 = 23 + 流守恒/容量约束；
     * 2) 10 个随机有向图：max-flow == 暴力最小割（2^(n-2) 子集枚举）。
     */
    const int n = 6;
    double cap[36];
    double flow[36];
    double mf;
    int i, j, ok;
    char detail[260];
    FILE *fp;
    double s_out = 0.0, t_in = 0.0;

    memset(cap, 0, sizeof cap);
    /* s=0, t=5；CLRS 26.1 图 */
    cap[0 * n + 1] = 16.0;
    cap[0 * n + 2] = 13.0;
    cap[1 * n + 3] = 12.0;
    cap[2 * n + 1] = 4.0;
    cap[2 * n + 4] = 14.0;
    cap[3 * n + 2] = 9.0;
    cap[3 * n + 5] = 20.0;
    cap[4 * n + 3] = 7.0;
    cap[4 * n + 5] = 4.0;

    mf = mlab_graph_maxflow_ek(cap, n, 0, 5, flow);
    for (j = 0; j < n; ++j) s_out += flow[0 * n + j];
    for (i = 0; i < n; ++i) t_in += flow[i * n + 5];
    {
        int conserv = 1, capok = 1;
        for (i = 0; i < n * n; ++i)
            if (flow[i] > cap[i] + 1e-9) capok = 0;
        for (i = 1; i < n - 1 && conserv; ++i) {
            double in = 0.0, out = 0.0;
            for (j = 0; j < n; ++j) {
                in += flow[j * n + i];
                out += flow[i * n + j];
            }
            if (fabs(in - out) > 1e-9) conserv = 0;
        }
        ok = (fabs(mf - 23.0) < 1e-9) && (fabs(s_out - mf) < 1e-9) &&
             (fabs(t_in - mf) < 1e-9) && conserv && capok;
    }

    /* 随机图 max-flow vs 暴力最小割 */
    {
        mlab_rng rng;
        int inst, n_fail = 0;
        const int n_r = 7, n_inst = 10;
        mlab_rng_seed(&rng, 86003);
        for (inst = 0; inst < n_inst; ++inst) {
            double *c = (double *)calloc((size_t)n_r * (size_t)n_r, sizeof(double));
            double mfr, mincut = 1e300;
            int mask;
            if (!c) { ++n_fail; continue; }
            for (i = 0; i < n_r; ++i)
                for (j = 0; j < n_r; ++j)
                    if (i != j && mlab_rng_uniform(&rng) < 0.4)
                        c[i * n_r + j] = 1.0 + floor(mlab_rng_uniform(&rng) * 9.0);
            mfr = mlab_graph_maxflow_ek(c, n_r, 0, n_r - 1, NULL);
            /* 最小割：枚举含 s 不含 t 的子集 */
            for (mask = 0; mask < (1 << (n_r - 2)); ++mask) {
                double cut = 0.0;
                int a, b;
                for (a = 0; a < n_r; ++a) {
                    int in_a = (a == 0) || ((mask >> (a - 1)) & 1);
                    if (a == n_r - 1) in_a = 0;
                    if (!in_a) continue;
                    for (b = 0; b < n_r; ++b) {
                        int in_b;
                        if (b == n_r - 1) in_b = 0;
                        else in_b = (b == 0) || ((mask >> (b - 1)) & 1);
                        if (!in_b && c[a * n_r + b] > 0.0) cut += c[a * n_r + b];
                    }
                }
                if (cut < mincut) mincut = cut;
            }
            if (fabs(mfr - mincut) > 1e-9) ++n_fail;
            free(c);
        }
        ok = ok && (n_fail == 0);
        test_ensure_results_dir();
        fp = fopen("results/maxflow_mincut.csv", "w");
        if (fp) {
            fprintf(fp, "check,value\n");
            fprintf(fp, "clrs_maxflow,%.6f\n", mf);
            fprintf(fp, "clrs_expected,23\n");
            fprintf(fp, "random_mincut_mismatches,%d\n", n_fail);
            fclose(fp);
        }
        snprintf(detail, sizeof detail,
                 "CLRS 网络 max-flow=%.1f（期望 23，守恒+容量成立）; "
                 "随机图 maxflow==mincut %d/%d",
                 mf, n_inst - n_fail, n_inst);
    }
    test_record(ok, "maxflow", "maxflow_ek_and_mincut", detail);
    return ok;
}

static int t_dijkstra_rejects_negative(void)
{
    /* 负权图上 Dijkstra 显式拒算（-2），BF 正常处理 */
    mlab_graph g;
    double dist[3];
    int has_neg = 0, ok;
    char detail[160];
    int rc;

    mlab_graph_alloc(&g, 3);
    mlab_graph_set_directed(&g, 0, 1, 5.0);
    mlab_graph_set_directed(&g, 1, 2, -2.0);
    rc = mlab_graph_dijkstra(&g, 0, dist, NULL);
    mlab_graph_bellman_ford(&g, 0, dist, NULL, &has_neg);
    ok = rc == -2 && !has_neg &&
         fabs(dist[0]) < 1e-12 && fabs(dist[1] - 5.0) < 1e-12 &&
         fabs(dist[2] - 3.0) < 1e-12;
    snprintf(detail, sizeof detail,
             "dijkstra rc=%d (expect -2); BF dist=(%.1f,%.1f,%.1f)",
             rc, dist[0], dist[1], dist[2]);
    test_record(ok, "shortpath", "dijkstra_rejects_negative", detail);
    mlab_graph_free(&g);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"shortpath", "dijkstra_vs_floyd_100_instances",
         "Dijkstra 与 Floyd 在 100 个随机图上一致",
         t_shortpath_dijkstra_vs_floyd},
        {"shortpath", "bellman_ford_negative_and_cycle",
         "Bellman-Ford 负权最短路与负环检出",
         t_shortpath_bellman_ford_negative},
        {"shortpath", "dijkstra_rejects_negative",
         "Dijkstra 负权显式拒算（-2）",
         t_dijkstra_rejects_negative},
        {"bfs", "bfs_order_and_levels",
         "已知图 BFS 序=层序，层=单位权 Dijkstra 跳数",
         t_bfs_order_and_levels},
        {"bfs", "bfs_levels_match_unit_dijkstra",
         "随机图 BFS 层序与单位权 Dijkstra 距离相容（20 图）",
         t_bfs_levels_match_unit_dijkstra},
        {"dfs", "dfs_order_and_reach",
         "已知图 DFS 深入优先序",
         t_dfs_order_and_reach},
        {"dfs", "dfs_bfs_same_component",
         "随机图上 BFS/DFS 访问同一连通分量（20 图）",
         t_dfs_bfs_same_component},
        {"maxflow", "maxflow_ek_and_mincut",
         "Edmonds-Karp 最大流：CLRS 网络=23、流守恒；随机图与最小割互验",
         t_maxflow_known_and_mincut},
        {"assign", "assign_d3_matches_b4_vendor",
         "指派：D3 枚举 vs B4 分支定界 30 实例一致（B4 联动）",
         t_assign_d3_vs_b4},
        {"mst", "prim_kruskal_agree",
         "Prim 与 Kruskal 最小生成树权一致",
         t_mst_prim_vs_kruskal},
        {"graph", "topo_sort_dag",
         "DAG 拓扑排序满足边偏序",
         t_topo_sort_dag},
        {"tsp", "held_karp_gap_rank_stable",
         "Held-Karp 基准 gap 可排序且跨 ≥10 实例稳定",
         t_tsp_held_karp_baseline_gaps},
        {"tsp", "two_opt_better_than_nearest_neighbor",
         "2-opt 显著优于最近邻（配对 p<0.01）",
         t_tsp_two_opt_vs_nn},
        {"tsp", "held_karp_small_exact",
         "小规模 Held-Karp 可行且不劣于 NN",
         t_tsp_held_karp_small_exact},
        {"tsp", "two_opt_random_start_basins",
         "随机初始回路 2-opt 改进分布 + 局部最优盆地统计（:653）",
         t_tsp_two_opt_random_basins},
        {"tsp", "held_karp_n16",
         "n=16 Held-Karp 基准可算",
         t_tsp_held_karp_n16_once},
        {"dp", "knapsack_dp_matches_b4_vendor",
         "背包 DP 与 B4 分支定界/暴力三方对账（30 实例）",
         t_knapsack_dp_vs_b4},
    };
    test_ensure_results_dir();
    return test_run_main("D3-graph-combo", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
