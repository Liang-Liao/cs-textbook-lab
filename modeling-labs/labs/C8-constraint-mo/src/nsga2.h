#ifndef MLAB_NSGA2_H
#define MLAB_NSGA2_H

#include "rng.h"

/* C8 NSGA-II + ZDT1 + IGD/HV */

#define MLAB_NSGA2_MAX_OBJ 2

typedef void (*mlab_mo_eval)(const double *x, int n, double *f, int nobj, void *ctx);

typedef struct {
    mlab_mo_eval eval;
    void *ctx;
    int dim;              /* 决策变量数 */
    int nobj;             /* 目标数，ZDT 系为 2 */
    const double *lb, *ub;
    int pop;              /* 种群大小 */
    int max_gen;
    double p_cross, eta_c;
    double p_mut, eta_m;
} mlab_nsga2_config;

typedef struct {
    int pop;
    int dim;
    int nobj;
    int gen_used;
    int n_eval;
    double *X;            /* pop * dim */
    double *F;            /* pop * nobj */
    int *rank;
    double *crowd;
    double *igd_hist;     /* 每代 IGD（调用方提供 ref 时） */
    int igd_len;
} mlab_nsga2_result;

void mlab_nsga2_result_free(mlab_nsga2_result *r);

/* ZDT1: f1=x1, g=1+9*sum(x2..xn)/(n-1), f2=g*(1-sqrt(f1/g)) */
void mlab_zdt1_eval(const double *x, int n, double *f, int nobj, void *ctx);
/* 真值前沿：f1∈[0,1], f2=1-sqrt(f1) */
void mlab_zdt1_true_pf(double *f1, double *f2, int npts);

/* IGD：ref 为 nref × nobj（行主序），set 为 np × nobj */
double mlab_igd(const double *ref, int nref, const double *set, int np, int nobj);

/* 超体积（2D 最小化）：ref 点 ref_f1/ref_f2；set 为 np×2，先按 f1 升序扫 */
double mlab_hv2d(const double *set, int np, double ref_f1, double ref_f2);

int mlab_nsga2_run(mlab_rng *rng, const mlab_nsga2_config *cfg,
                   const double *igd_ref, int nref,
                   mlab_nsga2_result *out);

#endif
