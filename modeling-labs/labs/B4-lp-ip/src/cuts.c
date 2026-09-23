#include "cuts.h"
#include "linalg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CUTS_MAX 12
#define TOL 1e-9

/* 约束集（含动态追加的割） */
typedef struct {
    double a[CUTS_MAX + 8][2];
    double b[CUTS_MAX + 8];
    int m;
} cons_set;

static double frac_part(double v)
{
    double f = v - floor(v);
    if (f < 1e-9) f = 0.0;
    if (f > 1.0 - 1e-9) f = 0.0;
    return f;
}

/* 枚举两两约束交点 + 坐标轴，返回可行最优（max c·x）。返回 0 找到。 */
static int lp_vertex_enum(const cons_set *cs, const double *c, double *x_best, double *obj_best)
{
    int i, j, found = 0;
    double best = -1e300;
    double xb[2] = {0.0, 0.0};
    /* 原点 */
    {
        int feas = 1;
        for (i = 0; i < cs->m; ++i)
            if (cs->a[i][0] * xb[0] + cs->a[i][1] * xb[1] > cs->b[i] + TOL) feas = 0;
        if (feas) {
            best = c[0] * xb[0] + c[1] * xb[1];
            found = 1;
        }
    }
    for (i = 0; i < cs->m; ++i)
        for (j = i + 1; j < cs->m; ++j) {
            double det = cs->a[i][0] * cs->a[j][1] - cs->a[i][1] * cs->a[j][0];
            double pt[2], obj;
            int k, feas = 1;
            if (fabs(det) < 1e-12) continue;
            pt[0] = (cs->b[i] * cs->a[j][1] - cs->a[i][1] * cs->b[j]) / det;
            pt[1] = (cs->a[i][0] * cs->b[j] - cs->b[i] * cs->a[j][0]) / det;
            if (pt[0] < -TOL || pt[1] < -TOL) feas = 0;
            for (k = 0; feas && k < cs->m; ++k)
                if (cs->a[k][0] * pt[0] + cs->a[k][1] * pt[1] > cs->b[k] + TOL) feas = 0;
            if (!feas) continue;
            obj = c[0] * pt[0] + c[1] * pt[1];
            if (obj > best) {
                best = obj;
                xb[0] = pt[0];
                xb[1] = pt[1];
                found = 1;
            }
        }
    /* 坐标轴上的交点：约束 i 与 x1=0 / x2=0 */
    for (i = 0; i < cs->m; ++i) {
        for (j = 0; j < 2; ++j) {
            double pt[2], obj;
            int k, feas = 1;
            if (fabs(cs->a[i][j]) < 1e-12) continue;
            pt[j] = cs->b[i] / cs->a[i][j];
            pt[1 - j] = 0.0;
            if (pt[0] < -TOL || pt[1] < -TOL) feas = 0;
            for (k = 0; feas && k < cs->m; ++k)
                if (cs->a[k][0] * pt[0] + cs->a[k][1] * pt[1] > cs->b[k] + TOL) feas = 0;
            if (!feas) continue;
            obj = c[0] * pt[0] + c[1] * pt[1];
            if (obj > best) {
                best = obj;
                xb[0] = pt[0];
                xb[1] = pt[1];
                found = 1;
            }
        }
    }
    if (found) {
        x_best[0] = xb[0];
        x_best[1] = xb[1];
        *obj_best = best;
    }
    return found ? 0 : 1;
}

int mlab_cuts_demo_2d(const double *A, const double *b, const double *c, int m,
                      double *x_out, double *obj_out, int *n_cuts_out)
{
    cons_set cs;
    int cut, rc = 1;
    double x[2] = {0.0, 0.0}, obj = 0.0;
    if (m <= 0 || !A || !b || !c || !x_out || !obj_out) return -1;
    if (m > CUTS_MAX + 4) return -1;
    for (cut = 0; cut < m; ++cut) {
        cs.a[cut][0] = A[cut * 2 + 0];
        cs.a[cut][1] = A[cut * 2 + 1];
        cs.b[cut] = b[cut];
    }
    cs.m = m;
    for (cut = 0; cut < CUTS_MAX; ++cut) {
        if (lp_vertex_enum(&cs, c, x, &obj) != 0) break;
        if (frac_part(x[0]) == 0.0 && frac_part(x[1]) == 0.0) {
            rc = 0; /* 整数最优 */
            break;
        }
        /*
         * 从当前分数顶点生成 Gomory 割：
         * 顶点 = 两条紧约束 (p,q) 的交点；以基本变量（x1/x2 或未紧行的 slack）
         * 的 tableau 行做纯整数舍入：Σ frac(系数)·非基本变量 >= frac(取值)，
         * 再借 s_i = b_i − a_i·x 转回 x 空间，并在可能时缩放为整数系数。
         */
        {
            double rows[8][3]; /* 紧约束行 (a1, a2, b) */
            int nr = 0, p, q, done = 0;
            for (p = 0; p < cs.m && nr < 8; ++p)
                if (fabs(cs.a[p][0] * x[0] + cs.a[p][1] * x[1] - cs.b[p]) < 1e-7) {
                    rows[nr][0] = cs.a[p][0];
                    rows[nr][1] = cs.a[p][1];
                    rows[nr][2] = cs.b[p];
                    ++nr;
                }
            if (nr < 2) break;
            for (p = 0; p < nr && !done; ++p)
                for (q = p + 1; q < nr && !done; ++q) {
                    double det = rows[p][0] * rows[q][1] - rows[p][1] * rows[q][0];
                    double B2inv[4], bbar[2];
                    int bi, i2;
                    if (fabs(det) < 1e-12) continue;
                    /* B2inv = adj([[a_p],[a_q]])/det */
                    B2inv[0] = rows[q][1] / det;
                    B2inv[1] = -rows[p][1] / det;
                    B2inv[2] = -rows[q][0] / det;
                    B2inv[3] = rows[p][0] / det;
                    bbar[0] = B2inv[0] * rows[p][2] + B2inv[1] * rows[q][2];
                    bbar[1] = B2inv[2] * rows[p][2] + B2inv[3] * rows[q][2];
                    /* 候选 1：基本变量 x1 / x2 的 tableau 行
                       x_bi = bbar_bi − (B2inv row bi)·(s_p, s_q) */
                    for (bi = 0; bi < 2 && !done; ++bi) {
                        double g[2], fbar;
                        double a1n, a2n, b0n;
                        fbar = frac_part(bbar[bi]);
                        if (fbar == 0.0) continue;
                        g[0] = frac_part(B2inv[bi * 2 + 0]);
                        g[1] = frac_part(B2inv[bi * 2 + 1]);
                        if (g[0] == 0.0 && g[1] == 0.0) continue;
                        /* s→x：g_p s_p + g_q s_q >= fbar，
                           s_k = rows[k].b − rows[k].a·x */
                        a1n = g[0] * rows[p][0] + g[1] * rows[q][0];
                        a2n = g[0] * rows[p][1] + g[1] * rows[q][1];
                        b0n = g[0] * rows[p][2] + g[1] * rows[q][2] - fbar;
                        {
                            double a1 = a1n, a2 = a2n, b0 = b0n;
                            int scale;
                            for (scale = 1; scale <= 200; ++scale) {
                                double s1 = a1n * scale, s2 = a2n * scale, s3 = b0n * scale;
                                if (fabs(s1 - round(s1)) < 1e-9 &&
                                    fabs(s2 - round(s2)) < 1e-9 &&
                                    fabs(s3 - round(s3)) < 1e-9) {
                                    a1 = round(s1);
                                    a2 = round(s2);
                                    b0 = round(s3);
                                    break;
                                }
                            }
                            if (b0 < 0) {
                                /* 归一为 <= 形式且 b>=0 便于顶点枚举 */
                                a1 = -a1; a2 = -a2; b0 = -b0;
                            }
                            cs.a[cs.m][0] = a1;
                            cs.a[cs.m][1] = a2;
                            cs.b[cs.m] = b0;
                            ++cs.m;
                            done = 1;
                        }
                    }
                    if (done) break;
                    /* 候选 2：未紧行 slack 的 tableau 行
                       s_i − (a_i·B2inv)·(s_p,s_q) = b_i − a_i·x* */
                    for (i2 = 0; i2 < cs.m && !done; ++i2) {
                        double value_i = cs.b[i2] - cs.a[i2][0] * x[0] - cs.a[i2][1] * x[1];
                        double fbar, rowvec[2], g[2];
                        double a1n, a2n, b0n;
                        if (fabs(value_i) < 1e-9) continue; /* 紧行：slack 非基本 */
                        fbar = frac_part(value_i);
                        if (fbar == 0.0) continue;
                        rowvec[0] = cs.a[i2][0] * B2inv[0] + cs.a[i2][1] * B2inv[2];
                        rowvec[1] = cs.a[i2][0] * B2inv[1] + cs.a[i2][1] * B2inv[3];
                        g[0] = frac_part(-rowvec[0]);
                        g[1] = frac_part(-rowvec[1]);
                        if (g[0] == 0.0 && g[1] == 0.0) continue;
                        a1n = g[0] * rows[p][0] + g[1] * rows[q][0];
                        a2n = g[0] * rows[p][1] + g[1] * rows[q][1];
                        b0n = g[0] * rows[p][2] + g[1] * rows[q][2] - fbar;
                        {
                            double a1 = a1n, a2 = a2n, b0 = b0n;
                            int scale;
                            for (scale = 1; scale <= 200; ++scale) {
                                double s1 = a1n * scale, s2 = a2n * scale, s3 = b0n * scale;
                                if (fabs(s1 - round(s1)) < 1e-9 &&
                                    fabs(s2 - round(s2)) < 1e-9 &&
                                    fabs(s3 - round(s3)) < 1e-9) {
                                    a1 = round(s1);
                                    a2 = round(s2);
                                    b0 = round(s3);
                                    break;
                                }
                            }
                            if (b0 < 0) {
                                a1 = -a1; a2 = -a2; b0 = -b0;
                            }
                            cs.a[cs.m][0] = a1;
                            cs.a[cs.m][1] = a2;
                            cs.b[cs.m] = b0;
                            ++cs.m;
                            done = 1;
                        }
                    }
                }
            if (!done) break;
        }
    }
    /* 最终解一次 LP（最后一批割生效后） */
    if (lp_vertex_enum(&cs, c, x, &obj) == 0) {
        x_out[0] = x[0];
        x_out[1] = x[1];
        *obj_out = obj;
        if (n_cuts_out) *n_cuts_out = cs.m - m;
    } else {
        rc = 1;
        if (n_cuts_out) *n_cuts_out = cs.m - m;
    }
    return rc;
}
