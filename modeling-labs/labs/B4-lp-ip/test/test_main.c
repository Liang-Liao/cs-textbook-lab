/*
 * B4: simplex + knapsack + assign + cuts 多场景
 * 全量运行（无参数）时重算 results/*.csv（固定 seed，确定性可复现）。
 */
#include "harness.h"
#include "simplex.h"
#include "knapsack.h"
#include "assign.h"
#include "cuts.h"
#include "rng.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- simplex ---- */

static int t_simplex_known_lp(void)
{
    /* max x1+x2 s.t. x1+x2+s1=2, x1+s2=1, x,s>=0
       min -x1-x2+0s → obj=-2 */
    double A[8] = {
        1, 1, 1, 0,
        1, 0, 0, 1
    };
    double b[2] = {2, 1};
    double c[4] = {-1, -1, 0, 0};
    double x[4] = {0}, obj = 1e9, y[2] = {0};
    int rc = mlab_simplex(A, 2, 4, b, c, x, &obj, y);
    int ok = (rc == 0) && fabs(obj + 2.0) < 1e-6;
    char d[80];
    snprintf(d, sizeof d, "rc=%d obj=%.6f x=(%.2f,%.2f,%.2f,%.2f) want obj=-2",
             rc, obj, x[0], x[1], x[2], x[3]);
    test_record(ok, "simplex", "max_sum_known_optimum", d);
    return ok;
}

static int t_simplex_nonneg_solution(void)
{
    /* 与 max_sum 同一 LP：检查解分量非负 */
    double A[8] = {1, 1, 1, 0, 1, 0, 0, 1};
    double b[2] = {2, 1};
    double c[4] = {-1, -1, 0, 0};
    double x[4] = {0}, obj = 0, y[2] = {0};
    int i, neg = 0, ok, rc;
    rc = mlab_simplex(A, 2, 4, b, c, x, &obj, y);
    for (i = 0; i < 4; ++i) if (x[i] < -1e-8) ++neg;
    ok = (rc == 0) && neg == 0 && fabs(obj + 2.0) < 1e-6;
    {
        char d[80];
        snprintf(d, sizeof d, "rc=%d neg=%d obj=%.6f x1+x2=%.4f", rc, neg, obj, x[0] + x[1]);
        test_record(ok, "simplex", "solution_components_nonnegative", d);
        return ok;
    }
}

/*
 * 对偶验证（路线图 L261/265）：min c'x, Ax=b, x>=0。
 * 判据：原始-对偶目标间隙 = 0（y·b == c·x）；互补松弛 x_j·(c_j - y·A_j) = 0。
 */
static int t_simplex_dual_gap_complementary(void)
{
    /* min -x1-x2 s.t. x1+x2+s1=1, s>=0 → x*=(1,0) obj=-1, y*=-1 */
    double A[3] = {1, 1, 1};
    double b[1] = {1};
    double c[3] = {-1, -1, 0};
    double x[3], obj = 0, y[1] = {0};
    int j, rc, ok;
    double gap = 0, comp = 0;
    rc = mlab_simplex(A, 1, 3, b, c, x, &obj, y);
    for (j = 0; j < 3; ++j) comp += fabs(x[j] * (c[j] - y[0] * A[j]));
    gap = fabs(y[0] * b[0] - obj);
    ok = rc == 0 && gap < 1e-12 && comp < 1e-9 && fabs(y[0] + 1.0) < 1e-9;
    {
        char d[112];
        snprintf(d, sizeof d, "y=%.6f gap=%.2e comp=%.2e（对偶符号与间隙）",
                 y[0], gap, comp);
        test_record(ok, "simplex", "dual_gap_and_complementary_slackness", d);
    }
    return ok;
}

/* ---- 顶点枚举互验（路线图 L260 实验 1） ---- */

static double g_enum_mismatch;

static int t_simplex_vs_vertex_enum(void)
{
    mlab_rng rng;
    const int R = 30, m = 3, n = 4;
    int s, mism = 0, ok;
    FILE *fp;
    fp = fopen("results/lp_compare.csv", "w");
    if (fp) fprintf(fp, "inst,simplex_obj,enum_obj,agree\n");
    mlab_rng_seed(&rng, 100);
    for (s = 0; s < R; ++s) {
        double A[12], b[3], c[4], x1[4], x2[4], o1 = 0, o2 = 0;
        int i;
        for (i = 0; i < m * n; ++i) A[i] = (double)((int)(mlab_rng_uniform(&rng) * 9) - 3);
        for (i = 0; i < m; ++i) {
            /* 保证 b>=0 且可行：以 x0=(1,1,1,1) 生成 b */
            b[i] = 0.0;
            {
                int j;
                for (j = 0; j < n; ++j) b[i] += A[i * n + j];
            }
            if (b[i] < 0) b[i] = 0.0;
        }
        for (i = 0; i < n; ++i) c[i] = -1.0 - mlab_rng_uniform(&rng) * 3.0;
        if (mlab_simplex(A, m, n, b, c, x1, &o1, NULL) != 0) continue;
        if (mlab_vertex_enum(A, m, n, b, c, x2, &o2) != 0) continue;
        {
            int agree = fabs(o1 - o2) < 1e-7;
            if (!agree) ++mism;
            if (fp)
                fprintf(fp, "%d,%.8f,%.8f,%d\n", s, o1, o2, agree);
        }
    }
    if (fp) fclose(fp);
    g_enum_mismatch = (double)mism / R;
    ok = mism == 0; /* 路线图 L265 精神：两法逐实例一致 */
    {
        char d[80];
        snprintf(d, sizeof d, "%d 个随机 LP：simplex vs 顶点枚举 不一致=%d", R, mism);
        test_record(ok, "simplex", "matches_vertex_enumeration", d);
    }
    return ok;
}

/* ---- knapsack ---- */

static int t_knapsack_small_enum(void)
{
    double w[3] = {2, 3, 4}, v[3] = {3, 4, 5};
    int sel[3] = {0};
    double best = mlab_knapsack_brute(w, v, 3, 5.0, sel);
    int ok = fabs(best - 7.0) < 1e-12;
    char d[64];
    snprintf(d, sizeof d, "brute best=%.1f want 7", best);
    test_record(ok, "knapsack", "tiny_brute_known", d);
    return ok;
}

static int t_knapsack_bb_matches_brute(void)
{
    mlab_rng rng;
    double w[12], v[12], bb, br;
    int i, s1[12], s2[12];
    mlab_rng_seed(&rng, 100);
    for (i = 0; i < 12; ++i) {
        w[i] = 1.0 + mlab_rng_uniform(&rng) * 5.0;
        v[i] = 1.0 + mlab_rng_uniform(&rng) * 10.0;
    }
    br = mlab_knapsack_brute(w, v, 12, 20.0, s2);
    bb = mlab_knapsack_bb(w, v, 12, 20.0, s1);
    {
        int ok = fabs(bb - br) < 1e-9;
        char d[64];
        snprintf(d, sizeof d, "bb=%.4f brute=%.4f", bb, br);
        test_record(ok, "knapsack", "bb_matches_brute_n12", d);
        return ok;
    }
}

static int t_knapsack_zero_capacity(void)
{
    double w[2] = {1, 2}, v[2] = {5, 6};
    int sel[2];
    double best = mlab_knapsack_brute(w, v, 2, 0.0, sel);
    int ok = fabs(best) < 1e-12;
    char d[32];
    snprintf(d, sizeof d, "best=%.1f", best);
    test_record(ok, "knapsack", "zero_capacity_value_zero", d);
    return ok;
}

/* 路线图 L262/266：分支定界解 100 个随机实例，与暴力枚举对账 */
static int g_ks_match;

static int t_knapsack_100_instances(void)
{
    mlab_rng rng;
    int s, match = 0, ok;
    FILE *fp;
    fp = fopen("results/knapsack_100.csv", "w");
    if (fp) fprintf(fp, "inst,n,cap,bb,brute,match\n");
    mlab_rng_seed(&rng, 101);
    for (s = 0; s < 100; ++s) {
        double w[10], v[10], cap = 0, bb, br;
        int sel1[10], sel2[10], i;
        for (i = 0; i < 10; ++i) {
            w[i] = 1.0 + mlab_rng_uniform(&rng) * 19.0;
            v[i] = 1.0 + mlab_rng_uniform(&rng) * 49.0;
            cap += w[i];
        }
        cap *= 0.4;
        bb = mlab_knapsack_bb(w, v, 10, cap, sel1);
        br = mlab_knapsack_brute(w, v, 10, cap, sel2);
        {
            int agree = fabs(bb - br) < 1e-9;
            if (agree) ++match;
            if (fp) fprintf(fp, "%d,10,%.2f,%.4f,%.4f,%d\n", s, cap, bb, br, agree);
        }
    }
    if (fp) fclose(fp);
    g_ks_match = match;
    ok = match == 100; /* 路线图 L266：100/100 实例解一致 */
    {
        char d[80];
        snprintf(d, sizeof d, "BnB vs 暴力：%d/100 一致", match);
        test_record(ok, "knapsack", "bnb_vs_brute_100_instances", d);
    }
    return ok;
}

/* ---- assign（指派，路线图 L262） ---- */

static int t_assign_bnb_matches_enum(void)
{
    mlab_rng rng;
    const int n = 6, R = 50;
    int s, mism = 0, ok;
    FILE *fp;
    fp = fopen("results/assign_50.csv", "w");
    if (fp) fprintf(fp, "inst,n,bnb,enum,agree\n");
    mlab_rng_seed(&rng, 202);
    for (s = 0; s < R; ++s) {
        double cost[36], vb, ve;
        int p1[6], p2[6], i;
        for (i = 0; i < n * n; ++i)
            cost[i] = 1.0 + (int)(mlab_rng_uniform(&rng) * 20.0);
        vb = mlab_assign_bnb(cost, n, p1);
        ve = mlab_assign_enum(cost, n, p2);
        {
            int agree = fabs(vb - ve) < 1e-9;
            if (!agree) ++mism;
            if (fp) fprintf(fp, "%d,%d,%.4f,%.4f,%d\n", s, n, vb, ve, agree);
        }
    }
    if (fp) fclose(fp);
    ok = mism == 0;
    {
        char d[80];
        snprintf(d, sizeof d, "n=%d %d 实例：BnB vs 全排列 不一致=%d", n, R, mism);
        test_record(ok, "assign", "bnb_matches_enum_n6", d);
    }
    return ok;
}

static int t_assign_known(void)
{
    double cost[9] = {
        4, 1, 3,
        2, 0, 5,
        3, 2, 2
    };
    int perm[3];
    double v = mlab_assign_bnb(cost, 3, perm);
    /* 最优：0→1(1), 1→0(2), 2→2(2) = 5 */
    int ok = fabs(v - 5.0) < 1e-12;
    char d[64];
    snprintf(d, sizeof d, "opt=%.1f perm=(%d,%d,%d) want 5", v, perm[0], perm[1], perm[2]);
    test_record(ok, "assign", "known_optimum_n3", d);
    return ok;
}

/* ---- cuts（Gomory 割平面演示） ---- */

static int t_cuts_demo_known_ilp(void)
{
    /* max 3x1+2x2 s.t. 2x1+x2<=7, x1+3x2<=9（整数系数），x 整数≥0
       LP 最优 (2.4,2.2) 分数 obj=11.6；整数最优 (3,1) obj=11。
       割平面收敛后与盒内暴力枚举对账。 */
    double A[4] = {2, 1, 1, 3};
    double b[2] = {7.0, 9.0};
    double c[2] = {3, 2};
    double x[2], obj = 0;
    int nc = 0, rc, ok, i1, i2;
    double brute = 0;
    rc = mlab_cuts_demo_2d(A, b, c, 2, x, &obj, &nc);
    for (i1 = 0; i1 <= 7; ++i1)
        for (i2 = 0; i2 <= 9; ++i2)
            if (2 * i1 + i2 <= 7 && i1 + 3 * i2 <= 9 && 3.0 * i1 + 2.0 * i2 > brute)
                brute = 3.0 * i1 + 2.0 * i2;
    ok = rc == 0 && fabs(obj - 11.0) < 1e-7 && fabs(obj - brute) < 1e-7 && nc >= 1;
    {
        char d[112];
        snprintf(d, sizeof d, "rc=%d x=(%.3f,%.3f) obj=%.4f cuts=%d brute=%.1f want 11",
                 rc, x[0], x[1], obj, nc, brute);
        test_record(ok, "cuts", "gomory_demo_known_ilp", d);
        return ok;
    }
}

static int t_cuts_demo_pure_integer_lp(void)
{
    /* 整数系数且 LP 最优已为整数：无需割，直接闭环 */
    double A[4] = {1, 0, 0, 1};
    double b[2] = {3.0, 4.0};
    double c[2] = {2, 5};
    double x[2], obj = 0;
    int nc = 0, rc, ok;
    rc = mlab_cuts_demo_2d(A, b, c, 2, x, &obj, &nc);
    ok = rc == 0 && fabs(obj - 26.0) < 1e-9 && nc == 0;
    {
        char d[80];
        snprintf(d, sizeof d, "rc=%d obj=%.4f cuts=%d want 26/0", rc, obj, nc);
        test_record(ok, "cuts", "integer_vertex_no_cut_needed", d);
        return ok;
    }
}

/* ---- results（knapsack_100 / lp_compare / assign_50 已在 case 内落盘） ---- */

static void write_csvs(void)
{
    FILE *fp = fopen("results/knapsack_summary.csv", "w");
    if (fp) {
        fprintf(fp, "instances,match\n");
        fprintf(fp, "100,%d\n", g_ks_match);
        fclose(fp);
    }
    fp = fopen("results/lp_enum_mismatch.csv", "w");
    if (fp) {
        fprintf(fp, "instances,mismatch_rate\n");
        fprintf(fp, "30,%.4f\n", g_enum_mismatch);
        fclose(fp);
    }
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"simplex", "max_sum_known_optimum", "简单 LP 已知最优", t_simplex_known_lp},
        {"simplex", "solution_components_nonnegative", "解分量非负", t_simplex_nonneg_solution},
        {"simplex", "dual_gap_and_complementary_slackness", "对偶间隙=0 与互补松弛（L265）", t_simplex_dual_gap_complementary},
        {"simplex", "matches_vertex_enumeration", "simplex vs 顶点枚举互验（L260）", t_simplex_vs_vertex_enum},
        {"knapsack", "tiny_brute_known", "小背包暴力已知解", t_knapsack_small_enum},
        {"knapsack", "bb_matches_brute_n12", "分支定界 vs 暴力", t_knapsack_bb_matches_brute},
        {"knapsack", "zero_capacity_value_zero", "零容量最优值为 0", t_knapsack_zero_capacity},
        {"knapsack", "bnb_vs_brute_100_instances", "100 随机实例对账（L262/266）", t_knapsack_100_instances},
        {"assign", "bnb_matches_enum_n6", "指派 BnB vs 全排列 50 实例（L262）", t_assign_bnb_matches_enum},
        {"assign", "known_optimum_n3", "指派已知最优", t_assign_known},
        {"cuts", "gomory_demo_known_ilp", "Gomory 割闭环已知 ILP（L257）", t_cuts_demo_known_ilp},
        {"cuts", "integer_vertex_no_cut_needed", "整数顶点无需割", t_cuts_demo_pure_integer_lp},
    };
    int rc;
    test_ensure_results_dir();
    rc = test_run_main("B4-lp-ip", cases,
                       (int)(sizeof cases / sizeof cases[0]), argc, argv);
    if (argc <= 1) write_csvs();
    return rc;
}
