#include "tsp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int mlab_tsp_euclidean(mlab_tsp *t, const double *x, const double *y, int n)
{
    int i, j;
    if (!t || !x || !y || n < 2) return -1;
    memset(t, 0, sizeof *t);
    t->n = n;
    t->x = (double *)malloc((size_t)n * sizeof(double));
    t->y = (double *)malloc((size_t)n * sizeof(double));
    t->dist = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!t->x || !t->y || !t->dist) {
        mlab_tsp_free(t);
        return -1;
    }
    memcpy(t->x, x, (size_t)n * sizeof(double));
    memcpy(t->y, y, (size_t)n * sizeof(double));
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double dx = x[i] - x[j];
            double dy = y[i] - y[j];
            t->dist[i * n + j] = sqrt(dx * dx + dy * dy);
        }
    }
    return 0;
}

int mlab_tsp_from_matrix(mlab_tsp *t, const double *dist, int n)
{
    if (!t || !dist || n < 2) return -1;
    memset(t, 0, sizeof *t);
    t->n = n;
    t->dist = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    if (!t->dist) return -1;
    memcpy(t->dist, dist, (size_t)n * (size_t)n * sizeof(double));
    return 0;
}

void mlab_tsp_free(mlab_tsp *t)
{
    if (!t) return;
    free(t->x);
    free(t->y);
    free(t->dist);
    t->x = t->y = t->dist = NULL;
    t->n = 0;
}

double mlab_tsp_tour_length(const mlab_tsp *t, const int *tour)
{
    int i;
    double s = 0.0;
    if (!t || !tour || t->n < 2) return 0.0;
    for (i = 0; i < t->n; ++i) {
        int a = tour[i];
        int b = tour[(i + 1) % t->n];
        s += t->dist[a * t->n + b];
    }
    return s;
}

double mlab_tsp_nearest_neighbor(const mlab_tsp *t, int start, int *tour)
{
    int n, i, cur, remaining;
    char *used;
    if (!t || !tour || t->n < 2) return -1.0;
    n = t->n;
    if (start < 0 || start >= n) start = 0;
    used = (char *)calloc((size_t)n, 1);
    if (!used) return -1.0;
    cur = start;
    tour[0] = start;
    used[start] = 1;
    for (i = 1; i < n; ++i) {
        int best = -1, j;
        double bd = MLAB_TSP_INF;
        for (j = 0; j < n; ++j) {
            double d;
            if (used[j]) continue;
            d = t->dist[cur * n + j];
            if (d < bd) {
                bd = d;
                best = j;
            }
        }
        if (best < 0) {
            free(used);
            return -1.0;
        }
        tour[i] = best;
        used[best] = 1;
        cur = best;
    }
    free(used);
    remaining = n;
    (void)remaining;
    return mlab_tsp_tour_length(t, tour);
}

/* 2-opt：翻转 tour[i..j]（j>i）后的回路长度增量评估 */
double mlab_tsp_two_opt(const mlab_tsp *t, const int *tour0, int *tour_out,
                        int *n_improve)
{
    int n, i, j, improved = 1;
    int *tour;
    int total_imp = 0;
    double len;
    if (!t || !tour0 || !tour_out || t->n < 4) {
        if (t && tour0 && tour_out)
            memcpy(tour_out, tour0, (size_t)t->n * sizeof(int));
        return t ? mlab_tsp_tour_length(t, tour0) : -1.0;
    }
    n = t->n;
    tour = (int *)malloc((size_t)n * sizeof(int));
    if (!tour) return -1.0;
    memcpy(tour, tour0, (size_t)n * sizeof(int));
    len = mlab_tsp_tour_length(t, tour);
    while (improved) {
        double best_delta = 0.0;
        int bi = -1, bj = -1;
        improved = 0;
        for (i = 0; i < n - 1; ++i) {
            for (j = i + 2; j < n; ++j) {
                int a = tour[i];
                int b = tour[i + 1];
                int c = tour[j];
                int d = tour[(j + 1) % n];
                double delta;
                if (i == 0 && j == n - 1) continue; /* 整环翻转无意义 */
                /* 2-opt: 去边 (a,b),(c,d)，加 (a,c),(b,d) */
                delta = t->dist[a * n + c] + t->dist[b * n + d]
                      - t->dist[a * n + b] - t->dist[c * n + d];
                if (delta < best_delta - 1e-12) {
                    best_delta = delta;
                    bi = i;
                    bj = j;
                }
            }
        }
        if (bi >= 0 && bj > bi) {
            /* 翻转 tour[bi+1 .. bj] */
            int L = bi + 1, R = bj;
            while (L < R) {
                int tmp = tour[L];
                tour[L] = tour[R];
                tour[R] = tmp;
                ++L;
                --R;
            }
            len += best_delta;
            improved = 1;
            ++total_imp;
        }
    }
    memcpy(tour_out, tour, (size_t)n * sizeof(int));
    free(tour);
    if (n_improve) *n_improve = total_imp;
    return len;
}

double mlab_tsp_nn_two_opt(const mlab_tsp *t, int *tour_out)
{
    int n, s, *tour_tmp;
    double best = 1e300;
    if (!t || !tour_out || t->n < 2) return -1.0;
    n = t->n;
    tour_tmp = (int *)malloc((size_t)n * sizeof(int));
    if (!tour_tmp) return -1.0;
    for (s = 0; s < n; ++s) {
        int *nn = (int *)malloc((size_t)n * sizeof(int));
        int *improved = (int *)malloc((size_t)n * sizeof(int));
        double len;
        if (!nn || !improved) {
            free(nn);
            free(improved);
            break;
        }
        if (mlab_tsp_nearest_neighbor(t, s, nn) < 0) {
            free(nn);
            free(improved);
            continue;
        }
        len = mlab_tsp_two_opt(t, nn, improved, NULL);
        if (len < best) {
            best = len;
            memcpy(tour_out, improved, (size_t)n * sizeof(int));
        }
        free(nn);
        free(improved);
    }
    free(tour_tmp);
    return best;
}

/*
 * Held-Karp: dp[S][j] = 从 0 出发、经过 S（含 j）、终点 j 的最短路径长
 * S 位掩码，城市 0 固定为起点（对称 TSP 无妨）。
 */
double mlab_tsp_held_karp(const mlab_tsp *t, int *tour)
{
    int n, full, S, i, j, k;
    double *dp;
    int *parent;
    double best;
    int end = -1;
    int *stack, sp;
    if (!t || t->n < 2) return -1.0;
    n = t->n;
    if (n > 20) return -1.0;
    full = 1 << (n - 1); /* 城市 1..n-1 的子集 */
    dp = (double *)malloc((size_t)full * (size_t)(n - 1) * sizeof(double));
    parent = (int *)malloc((size_t)full * (size_t)(n - 1) * sizeof(int));
    if (!dp || !parent) {
        free(dp);
        free(parent);
        return -1.0;
    }
    for (S = 0; S < full; ++S)
        for (j = 0; j < n - 1; ++j) {
            dp[S * (n - 1) + j] = 1e300;
            parent[S * (n - 1) + j] = -1;
        }
    /* 基例：0 -> j */
    for (j = 1; j < n; ++j) {
        int bit = 1 << (j - 1);
        dp[bit * (n - 1) + (j - 1)] = t->dist[0 * n + j];
    }
    for (S = 1; S < full; ++S) {
        for (j = 1; j < n; ++j) {
            int jb = 1 << (j - 1);
            int Sj;
            double cur;
            if (!(S & jb)) continue;
            Sj = S ^ jb;
            cur = dp[S * (n - 1) + (j - 1)];
            if (Sj == 0) continue;
            for (k = 1; k < n; ++k) {
                int kb = 1 << (k - 1);
                double cand;
                if (!(Sj & kb)) continue;
                cand = dp[Sj * (n - 1) + (k - 1)] + t->dist[k * n + j];
                if (cand < cur) {
                    cur = cand;
                    parent[S * (n - 1) + (j - 1)] = k;
                    dp[S * (n - 1) + (j - 1)] = cur;
                }
            }
        }
    }
    best = 1e300;
    S = full - 1;
    for (j = 1; j < n; ++j) {
        double cand = dp[S * (n - 1) + (j - 1)] + t->dist[j * n + 0];
        if (cand < best) {
            best = cand;
            end = j;
        }
    }
    if (end < 0) {
        free(dp);
        free(parent);
        return -1.0;
    }
    if (tour) {
        stack = (int *)malloc((size_t)n * sizeof(int));
        if (stack) {
            int cur = end;
            int curs = full - 1;
            sp = 0;
            while (cur > 0) {
                stack[sp++] = cur;
                {
                    int p = parent[curs * (n - 1) + (cur - 1)];
                    int nbits = curs ^ (1 << (cur - 1));
                    if (p < 0) {
                        /* 基例 */
                        break;
                    }
                    cur = p;
                    curs = nbits;
                }
            }
            tour[0] = 0;
            for (i = 0; i < sp; ++i)
                tour[1 + i] = stack[sp - 1 - i];
            free(stack);
        }
    }
    free(dp);
    free(parent);
    return best;
}
