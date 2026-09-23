/*
 * M3: 端到端参数估计与不确定性量化
 * Lotka-Volterra + M2 式混合点估计 + C2 MCMC + 95% CrI 覆盖率 + 一键报告
 */
#include "harness.h"
#include "lab.h"
#include "e2e.h"
#include "rng.h"
#include "stats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static int file_exists(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int t_pipeline_full(void)
{
    mlab_e2e_config cfg;
    mlab_e2e_obs obs;
    mlab_e2e_posterior post;
    mlab_rng rng;
    double th[MLAB_E2E_NPAR];
    double fmin = 0.0;
    int ne = 0, j, ok, ok_cri = 0, ok_files;
    char detail[280];

    test_ensure_results_dir();
    mlab_e2e_default_lv(&cfg);
    mlab_rng_seed(&rng, 300101);
    memset(&obs, 0, sizeof obs);
    memset(&post, 0, sizeof post);
    memset(th, 0, sizeof th);

    if (mlab_e2e_pipeline(&rng, &cfg, 301001u, 302001u,
                          900, 1500, 2500, 3, 4,
                          "results", th, &fmin, &ne, &obs, &post) != 0) {
        test_record(0, "pipeline", "e2e_lv_full_pipeline", "pipeline failed");
        return 0;
    }

    ok_files = file_exists("results/m3_obs.csv") &&
               file_exists("results/m3_theta.csv") &&
               file_exists("results/m3_mcmc_samples.csv") &&
               file_exists("results/m3_report.md");

    for (j = 0; j < cfg.npar; ++j) {
        if (cfg.theta_true[j] >= post.cri_lo[j] && cfg.theta_true[j] <= post.cri_hi[j])
            ++ok_cri;
    }
    ok = ok_files && ok_cri >= 3 && fmin < 1e5 &&
         post.accept_rate > 0.05 && post.accept_rate < 0.95;
    snprintf(detail, sizeof detail,
             "θ̂=(%.4f,%.4f,%.4f,%.4f) true=(1,0.08,0.6,0.04) CrI hits=%d/4 "
             "R̂=%.3f acc=%.3f files=%d n_eval=%d nll=%.3f",
             th[0], th[1], th[2], th[3], ok_cri,
             post.rhat, post.accept_rate, ok_files, ne, fmin);
    test_record(ok, "pipeline", "e2e_lv_full_pipeline", detail);

    mlab_e2e_obs_free(&obs);
    mlab_e2e_posterior_free(&post);
    return ok;
}

static int t_point_est_recovers(void)
{
    mlab_e2e_config cfg;
    mlab_e2e_like_ctx like;
    mlab_e2e_obs obs;
    mlab_rng rng;
    double th[MLAB_E2E_NPAR];
    double fmin = 0.0;
    int ne = 0, j, ok = 1;
    char detail[240];
    double max_rel = 0.0;

    mlab_e2e_default_lv(&cfg);
    mlab_rng_seed(&rng, 310101);
    if (mlab_e2e_synth(&rng, &cfg, &obs) != 0) {
        test_record(0, "point_est", "hybrid_recovers_lv_params", "synth fail");
        return 0;
    }
    memset(&like, 0, sizeof like);
    like.obs = &obs;
    like.y0 = cfg.y0;
    like.sigma = cfg.sigma;
    like.h_ode = cfg.h_ode;
    memcpy(like.lb, cfg.lb, sizeof like.lb);
    memcpy(like.ub, cfg.ub, sizeof like.ub);
    like.model_id = cfg.model_id;

    if (mlab_e2e_point_est(&rng, mlab_e2e_nll, &like,
                           cfg.npar, cfg.lb, cfg.ub,
                           800, th, &fmin, &ne) != 0) {
        mlab_e2e_obs_free(&obs);
        test_record(0, "point_est", "hybrid_recovers_lv_params", "point_est fail");
        return 0;
    }
    for (j = 0; j < cfg.npar; ++j) {
        double scale = fmax(1e-6, fabs(cfg.theta_true[j]));
        double rel = fabs(th[j] - cfg.theta_true[j]) / scale;
        if (rel > max_rel) max_rel = rel;
        /* 噪声下允许相对误差 25%；α/γ 通常更准 */
        if (rel > 0.25) ok = 0;
    }
    snprintf(detail, sizeof detail,
             "θ̂=(%.4f,%.4f,%.4f,%.4f) max_rel_err=%.3f nll=%.3f ne=%d",
             th[0], th[1], th[2], th[3], max_rel, fmin, ne);
    test_record(ok, "point_est", "hybrid_recovers_lv_params", detail);
    mlab_e2e_obs_free(&obs);
    return ok;
}

static int t_mcmc_cri_rhat(void)
{
    mlab_e2e_config cfg;
    mlab_e2e_obs obs;
    mlab_e2e_posterior post;
    mlab_rng rng;
    double th[MLAB_E2E_NPAR];
    double fmin = 0.0;
    int ne = 0, j, hits = 0, ok;
    char detail[260];

    mlab_e2e_default_lv(&cfg);
    mlab_rng_seed(&rng, 320101);
    memset(&obs, 0, sizeof obs);
    memset(&post, 0, sizeof post);
    if (mlab_e2e_pipeline(&rng, &cfg, 321001u, 322001u,
                          700, 2500, 4000, 3, 4,
                          NULL, th, &fmin, &ne, &obs, &post) != 0) {
        test_record(0, "mcmc", "posterior_cri_and_rhat", "pipeline fail");
        return 0;
    }
    for (j = 0; j < cfg.npar; ++j)
        if (cfg.theta_true[j] >= post.cri_lo[j] && cfg.theta_true[j] <= post.cri_hi[j])
            ++hits;
    ok = (post.rhat < 1.1) && (hits >= 3) &&
         (post.accept_rate >= 0.08 && post.accept_rate <= 0.85);
    snprintf(detail, sizeof detail,
             "R̂=%.4f CrI hits=%d/4 acc=%.3f sd=(%.4f,%.4f,%.4f,%.4f)",
             post.rhat, hits, post.accept_rate,
             post.sd[0], post.sd[1], post.sd[2], post.sd[3]);
    test_record(ok, "mcmc", "posterior_cri_and_rhat", detail);
    mlab_e2e_obs_free(&obs);
    mlab_e2e_posterior_free(&post);
    return ok;
}

static int t_point_within_2sd(void)
{
    mlab_e2e_config cfg;
    mlab_e2e_obs obs;
    mlab_e2e_posterior post;
    mlab_rng rng;
    double th[MLAB_E2E_NPAR];
    double fmin = 0.0;
    int ne = 0, j, n_ok = 0, ok;
    char detail[280];

    mlab_e2e_default_lv(&cfg);
    mlab_rng_seed(&rng, 330101);
    memset(&obs, 0, sizeof obs);
    memset(&post, 0, sizeof post);
    if (mlab_e2e_pipeline(&rng, &cfg, 331001u, 332001u,
                          900, 1500, 2500, 3, 4,
                          NULL, th, &fmin, &ne, &obs, &post) != 0) {
        test_record(0, "mcmc", "point_err_within_2_posterior_sd", "pipeline fail");
        return 0;
    }
    for (j = 0; j < cfg.npar; ++j) {
        double err = fabs(th[j] - cfg.theta_true[j]);
        double lim = 2.0 * post.sd[j];
        if (post.sd[j] > 1e-12 && err < lim) ++n_ok;
    }
    ok = (n_ok == cfg.npar);
    snprintf(detail, sizeof detail,
             "|err|<2sd: %d/4 | ratios=(%.2f,%.2f,%.2f,%.2f)",
             n_ok,
             fabs(th[0] - cfg.theta_true[0]) / fmax(post.sd[0], 1e-15),
             fabs(th[1] - cfg.theta_true[1]) / fmax(post.sd[1], 1e-15),
             fabs(th[2] - cfg.theta_true[2]) / fmax(post.sd[2], 1e-15),
             fabs(th[3] - cfg.theta_true[3]) / fmax(post.sd[3], 1e-15));
    test_record(ok, "mcmc", "point_err_within_2_posterior_sd", detail);
    mlab_e2e_obs_free(&obs);
    mlab_e2e_posterior_free(&post);
    return ok;
}

static int t_coverage(void)
{
    /*
     * 判据 1：95% CrI 真值覆盖率 ∈ [88%, 99%]（多次重复）。
     * 判据 2（R7 扩展口径）：点估计误差 < 2×后验 sd 按 40 次重复逐 rep 报告，
     *   给出 err<2sd 比例与反例 rep 号（如 rep36），不聚合掩没。
     * 口径：4 参数边际覆盖率的平均值；同时落盘每参数与联合率。
     */
    const int n_rep = 40;
    mlab_e2e_config cfg;
    mlab_rng rng;
    int hits[MLAB_E2E_NPAR];
    int joint = 0, r, j, ok;
    int n_ok2 = 0;
    char failreps[160];
    double rates[MLAB_E2E_NPAR], avg, prop2;
    FILE *fp;
    char detail[300];

    mlab_e2e_default_lv(&cfg);
    memset(hits, 0, sizeof hits);
    failreps[0] = '\0';
    test_ensure_results_dir();
    fp = fopen("results/m3_coverage.csv", "w");
    if (fp)
        fprintf(fp, "rep,nll,hat0,hat1,hat2,hat3,sd0,sd1,sd2,sd3,"
                    "hit0,hit1,hit2,hit3,joint,rhat,acc,"
                    "ok0,ok1,ok2,ok3,max_ratio\n");

    mlab_rng_seed(&rng, 340101);
    for (r = 0; r < n_rep; ++r) {
        mlab_e2e_config c = cfg;
        mlab_e2e_obs obs;
        mlab_e2e_posterior post;
        mlab_e2e_like_ctx like;
        double th[MLAB_E2E_NPAR];
        double fmin = 0.0;
        unsigned mseed = 360000u + (unsigned)r * 19u;
        int ne = 0, hitj = 1, okj2 = 1;
        double max_ratio = 0.0;
        mlab_rng local;

        memset(&obs, 0, sizeof obs);
        memset(&post, 0, sizeof post);
        mlab_rng_seed(&local, 370000u + (unsigned)r * 31u);
        if (mlab_e2e_synth(&local, &c, &obs) != 0) continue;
        memset(&like, 0, sizeof like);
        like.obs = &obs;
        like.y0 = c.y0;
        like.sigma = c.sigma;
        like.h_ode = c.h_ode;
        memcpy(like.lb, c.lb, sizeof like.lb);
        memcpy(like.ub, c.ub, sizeof like.ub);
        like.model_id = c.model_id;

        if (mlab_e2e_point_est(&local, mlab_e2e_nll, &like,
                               c.npar, c.lb, c.ub,
                               700, th, &fmin, &ne) != 0) {
            mlab_e2e_obs_free(&obs);
            continue;
        }
        mlab_rng_seed(&local, mseed);
        if (mlab_e2e_mcmc(&local, &like, th,
                          1000, 2000, 2, 1, 0.0, &post) != 0) {
            mlab_e2e_obs_free(&obs);
            continue;
        }
        for (j = 0; j < c.npar; ++j) {
            int hit = (c.theta_true[j] >= post.cri_lo[j] &&
                       c.theta_true[j] <= post.cri_hi[j]);
            int ok2 = 0;
            double ratio = 0.0;
            hits[j] += hit;
            if (!hit) hitj = 0;
            if (post.sd[j] > 1e-12) {
                ratio = fabs(th[j] - c.theta_true[j]) / post.sd[j];
                ok2 = (ratio < 2.0);
            }
            if (ratio > max_ratio) max_ratio = ratio;
            if (!ok2) okj2 = 0;
            n_ok2 += ok2;
        }
        joint += hitj;
        if (!okj2 && strlen(failreps) < 120) {
            char one[16];
            snprintf(one, sizeof one, "%srep%d", failreps[0] ? " " : "", r);
            strncat(failreps, one, sizeof failreps - strlen(failreps) - 1);
        }
        if (fp) {
            char rhat_buf[16];
            if (post.n_chains >= 2)
                snprintf(rhat_buf, sizeof rhat_buf, "%.4f", post.rhat);
            else
                snprintf(rhat_buf, sizeof rhat_buf, "NA"); /* 单链 R̂ 不适用（R7） */
            fprintf(fp, "%d,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%d,%d,%d,%d,%d,%s,%.3f,"
                        "%d,%d,%d,%d,%.3f\n",
                    r, fmin, th[0], th[1], th[2], th[3],
                    post.sd[0], post.sd[1], post.sd[2], post.sd[3],
                    (c.theta_true[0] >= post.cri_lo[0] && c.theta_true[0] <= post.cri_hi[0]),
                    (c.theta_true[1] >= post.cri_lo[1] && c.theta_true[1] <= post.cri_hi[1]),
                    (c.theta_true[2] >= post.cri_lo[2] && c.theta_true[2] <= post.cri_hi[2]),
                    (c.theta_true[3] >= post.cri_lo[3] && c.theta_true[3] <= post.cri_hi[3]),
                    hitj,
                    rhat_buf,
                    post.accept_rate,
                    (fabs(th[0] - c.theta_true[0]) < 2.0 * post.sd[0]),
                    (fabs(th[1] - c.theta_true[1]) < 2.0 * post.sd[1]),
                    (fabs(th[2] - c.theta_true[2]) < 2.0 * post.sd[2]),
                    (fabs(th[3] - c.theta_true[3]) < 2.0 * post.sd[3]),
                    max_ratio);
        }
        mlab_e2e_obs_free(&obs);
        mlab_e2e_posterior_free(&post);
    }
    if (fp) fclose(fp);

    avg = 0.0;
    ok = 1;
    for (j = 0; j < cfg.npar; ++j) {
        rates[j] = (n_rep > 0) ? (double)hits[j] / (double)n_rep : 0.0;
        avg += rates[j];
        if (rates[j] < 0.80 || rates[j] > 1.0) ok = 0; /* 单参数灾难性偏离 */
    }
    avg /= cfg.npar;
    if (avg < 0.88 || avg > 0.99) ok = 0;
    prop2 = (n_rep > 0) ? (double)n_ok2 / ((double)n_rep * cfg.npar) : 0.0;
    snprintf(detail, sizeof detail,
             "n_rep=%d avg CrI coverage=%.3f (per-param %.3f/%.3f/%.3f/%.3f) joint=%.3f | "
             "err<2sd 占比=%d/%d=%.3f 反例: %s",
             n_rep, avg, rates[0], rates[1], rates[2], rates[3],
             (n_rep > 0) ? (double)joint / n_rep : 0.0,
             n_ok2, n_rep * cfg.npar, prop2,
             failreps[0] ? failreps : "无");
    test_record(ok, "coverage", "cri_coverage_reps", detail);
    return ok;
}

static int t_report_reproducible(void)
{
    /* 同种子重跑流水线：中间产物可复算。
     * R7：repro 用例产物写入独立子目录 results/repro，不再覆盖主管线产物 */
    mlab_e2e_config cfg;
    mlab_e2e_obs obs1, obs2;
    mlab_e2e_posterior p1, p2;
    mlab_rng rng1, rng2;
    double th1[MLAB_E2E_NPAR], th2[MLAB_E2E_NPAR];
    double f1 = 0, f2 = 0;
    int ne1 = 0, ne2 = 0, j, ok;
    double max_dth = 0.0, max_dcri = 0.0;
    char detail[240];

    mlab_e2e_default_lv(&cfg);
    /* R7：独立产物目录，避免覆盖 t_pipeline_full 写入的 results/m3_* */
#ifdef _WIN32
    _mkdir("results/repro");
#else
    mkdir("results/repro", 0777);
#endif
    memset(&obs1, 0, sizeof obs1);
    memset(&obs2, 0, sizeof obs2);
    memset(&p1, 0, sizeof p1);
    memset(&p2, 0, sizeof p2);

    mlab_rng_seed(&rng1, 380101);
    mlab_rng_seed(&rng2, 380101);
    if (mlab_e2e_pipeline(&rng1, &cfg, 381001u, 382001u,
                          600, 800, 1000, 2, 2,
                          "results/repro", th1, &f1, &ne1, &obs1, &p1) != 0) {
        test_record(0, "report", "artifacts_reproducible", "run1 fail");
        return 0;
    }
    if (mlab_e2e_pipeline(&rng2, &cfg, 381001u, 382001u,
                          600, 800, 1000, 2, 2,
                          NULL, th2, &f2, &ne2, &obs2, &p2) != 0) {
        mlab_e2e_obs_free(&obs1);
        mlab_e2e_posterior_free(&p1);
        test_record(0, "report", "artifacts_reproducible", "run2 fail");
        return 0;
    }
    for (j = 0; j < cfg.npar; ++j) {
        double d = fabs(th1[j] - th2[j]);
        double dc = fabs(p1.cri_lo[j] - p2.cri_lo[j]) + fabs(p1.cri_hi[j] - p2.cri_hi[j]);
        if (d > max_dth) max_dth = d;
        if (dc > max_dcri) max_dcri = dc;
    }
    ok = (max_dth < 1e-8) && (max_dcri < 1e-6) &&
         file_exists("results/repro/m3_report.md") &&
         file_exists("results/repro/m3_obs.csv") &&
         file_exists("results/repro/m3_theta.csv") &&
         file_exists("results/m3_report.md") &&
         file_exists("results/m3_obs.csv") &&
         file_exists("results/m3_theta.csv");
    snprintf(detail, sizeof detail,
             "max|Δθ̂|=%.3g max|ΔCrI|=%.3g nll1=%.4f nll2=%.4f",
             max_dth, max_dcri, f1, f2);
    test_record(ok, "report", "artifacts_reproducible", detail);
    mlab_e2e_obs_free(&obs1);
    mlab_e2e_obs_free(&obs2);
    mlab_e2e_posterior_free(&p1);
    mlab_e2e_posterior_free(&p2);
    return ok;
}

int main(int argc, char **argv)
{
    static const test_case cases[] = {
        {"pipeline", "e2e_lv_full_pipeline",
         "合成观测→混合点估计→MCMC→CrI→报告落盘",
         t_pipeline_full},
        {"point_est", "hybrid_recovers_lv_params",
         "M2 式混合点估计恢复 LV 真参数",
         t_point_est_recovers},
        {"mcmc", "posterior_cri_and_rhat",
         "后验 95% CrI 含真值且 R̂<1.1",
         t_mcmc_cri_rhat},
        {"mcmc", "point_err_within_2_posterior_sd",
         "点估计误差 < 2 倍后验标准差",
         t_point_within_2sd},
        {"coverage", "cri_coverage_reps",
         "多次重复 95% CrI 真值覆盖率 ∈ [88%,99%]",
         t_coverage},
        {"report", "artifacts_reproducible",
         "同种子重跑中间产物可复算",
         t_report_reproducible},
    };
    test_ensure_results_dir();
    return test_run_main("M3-e2e-param-est", cases,
                         (int)(sizeof cases / sizeof cases[0]), argc, argv);
}
