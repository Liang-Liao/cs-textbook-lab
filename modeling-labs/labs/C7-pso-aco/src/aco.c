#include "aco.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mlab_aco_result_free(mlab_aco_result *r)
{
    if (!r) return;
    free(r->best_tour);
    r->best_tour = NULL;
}

double mlab_tsp_greedy_nn(const mlab_tsp_inst *inst, mlab_rng *rng,
                          int n_starts, int *tour_out)
{
    int n, s, i, k;
    int *tour, *cand, *used;
    double best = 1e300;

    if (!inst || inst->n < 3) return 1e300;
    n = inst->n;
    if (n_starts < 1) n_starts = 1;
    if (n_starts > n) n_starts = n;
    tour = (int *)malloc((size_t)n * sizeof(int));
    cand = (int *)malloc((size_t)n * sizeof(int));
    used = (int *)malloc((size_t)n * sizeof(int));
    if (!tour || !cand || !used) {
        free(tour); free(cand); free(used);
        return 1e300;
    }
    for (s = 0; s < n_starts; ++s) {
        int start = s;
        double len;
        if (rng && n_starts < n)
            start = (int)(mlab_rng_uniform(rng) * n) % n;
        memset(used, 0, (size_t)n * sizeof(int));
        cand[0] = start;
        used[start] = 1;
        for (k = 1; k < n; ++k) {
            int prev = cand[k - 1], nxt = -1;
            double bd = 1e300;
            for (i = 0; i < n; ++i) {
                double d;
                if (used[i]) continue;
                if (inst->dist)
                    d = inst->dist[prev * n + i];
                else {
                    double dx = inst->x[prev] - inst->x[i];
                    double dy = inst->y[prev] - inst->y[i];
                    d = sqrt(dx * dx + dy * dy);
                }
                if (d < bd) { bd = d; nxt = i; }
            }
            if (nxt < 0) break;
            cand[k] = nxt;
            used[nxt] = 1;
        }
        len = mlab_tsp_length(inst, cand);
        if (len < best) {
            best = len;
            memcpy(tour, cand, (size_t)n * sizeof(int));
        }
    }
    if (tour_out)
        memcpy(tour_out, tour, (size_t)n * sizeof(int));
    free(tour); free(cand); free(used);
    return best;
}

double mlab_tsp_two_opt_refine(const mlab_tsp_inst *inst, int *tour,
                               int max_pass, int *n_eval)
{
    int n, pass, i, j, improved = 1;
    double len;

    if (!inst || !tour || inst->n < 4) {
        return inst ? mlab_tsp_length(inst, tour) : 0.0;
    }
    n = inst->n;
    if (max_pass <= 0) max_pass = 2;
    len = mlab_tsp_length(inst, tour);
    if (n_eval) ++(*n_eval);
    for (pass = 0; pass < max_pass && improved; ++pass) {
        improved = 0;
        for (i = 0; i < n - 1; ++i) {
            for (j = i + 2; j < n; ++j) {
                double nl;
                if (i == 0 && j == n - 1) continue;
                mlab_tsp_apply_2opt(tour, n, i, j);
                nl = mlab_tsp_length(inst, tour);
                if (n_eval) ++(*n_eval);
                if (nl + 1e-12 < len) {
                    len = nl;
                    improved = 1;
                } else {
                    /* 回退：再翻转一次 */
                    mlab_tsp_apply_2opt(tour, n, i, j);
                }
            }
        }
    }
    return len;
}

int mlab_aco_tsp(mlab_rng *rng, const mlab_tsp_inst *inst,
                 const mlab_aco_config *cfg, mlab_aco_result *out)
{
    int n, nants, it, a, i, j, iter;
    double *tau = NULL, *eta = NULL;
    int *visited = NULL;
    int **ant_tours = NULL;
    double *ant_len = NULL;
    int *best_tour = NULL;
    double best_len = 1e300, tau0, q_deposit;
    double alpha, beta, rho;
    int elitist, use_2opt, two_opt_pass, mmas;
    int n_tours = 0, n_2opt = 0;

    if (!rng || !inst || !cfg || !out || inst->n < 3) return 0;
    n = inst->n;
    nants = cfg->n_ants > 0 ? cfg->n_ants : 20;
    if (nants > n) nants = n;
    iter = cfg->max_iter > 0 ? cfg->max_iter : 100;
    alpha = cfg->alpha > 0 ? cfg->alpha : 1.0;
    beta = cfg->beta > 0 ? cfg->beta : 2.0;
    rho = cfg->rho > 0 ? cfg->rho : 0.3;
    if (rho > 0.95) rho = 0.95;
    elitist = cfg->elitist;
    mmas = cfg->mmas;
    use_2opt = cfg->use_2opt;
    two_opt_pass = cfg->two_opt_pass > 0 ? cfg->two_opt_pass : 2;

    memset(out, 0, sizeof *out);

    tau = (double *)malloc((size_t)n * n * sizeof(double));
    eta = (double *)malloc((size_t)n * n * sizeof(double));
    visited = (int *)malloc((size_t)n * sizeof(int));
    best_tour = (int *)malloc((size_t)n * sizeof(int));
    ant_tours = (int **)malloc((size_t)nants * sizeof(int *));
    ant_len = (double *)malloc((size_t)nants * sizeof(double));
    if (!tau || !eta || !visited || !best_tour || !ant_tours || !ant_len) {
        free(tau); free(eta); free(visited); free(best_tour);
        free(ant_tours); free(ant_len);
        return 0;
    }
    for (a = 0; a < nants; ++a) {
        ant_tours[a] = (int *)malloc((size_t)n * sizeof(int));
        if (!ant_tours[a]) {
            int b;
            for (b = 0; b < a; ++b) free(ant_tours[b]);
            free(tau); free(eta); free(visited); free(best_tour);
            free(ant_tours); free(ant_len);
            return 0;
        }
    }

    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            double d;
            if (i == j) {
                eta[i * n + j] = 0.0;
                continue;
            }
            if (inst->dist)
                d = inst->dist[i * n + j];
            else {
                double dx = inst->x[i] - inst->x[j];
                double dy = inst->y[i] - inst->y[j];
                d = sqrt(dx * dx + dy * dy);
            }
            eta[i * n + j] = d > 1e-12 ? 1.0 / d : 1e6;
        }
    }

    tau0 = cfg->tau0;
    if (tau0 <= 0.0) {
        double g = mlab_tsp_greedy_nn(inst, rng, n, NULL);
        if (g <= 0) g = (double)n;
        tau0 = (double)nants / g;
        if (tau0 <= 0) tau0 = 1.0;
    }
    for (i = 0; i < n * n; ++i) tau[i] = tau0;
    q_deposit = (double)nants;

    out->best_tour = best_tour;
    for (it = 0; it < iter; ++it) {
        double iter_best = 1e300;
        int iter_best_a = 0;
        for (a = 0; a < nants; ++a) {
            int cur, step;
            memset(visited, 0, (size_t)n * sizeof(int));
            cur = (int)(mlab_rng_uniform(rng) * n) % n;
            ant_tours[a][0] = cur;
            visited[cur] = 1;
            for (step = 1; step < n; ++step) {
                double sum = 0.0, u, acc = 0.0;
                int nxt = -1;
                for (j = 0; j < n; ++j) {
                    double p;
                    if (visited[j]) continue;
                    p = pow(tau[cur * n + j], alpha) * pow(eta[cur * n + j], beta);
                    if (p < 0) p = 0;
                    sum += p;
                }
                if (sum <= 0.0) {
                    /* 回退：任选未访问 */
                    for (j = 0; j < n; ++j)
                        if (!visited[j]) { nxt = j; break; }
                } else {
                    u = mlab_rng_uniform(rng) * sum;
                    for (j = 0; j < n; ++j) {
                        double p;
                        if (visited[j]) continue;
                        p = pow(tau[cur * n + j], alpha) * pow(eta[cur * n + j], beta);
                        acc += p;
                        if (u <= acc || j == n - 1) { nxt = j; break; }
                    }
                    if (nxt < 0 || visited[nxt]) {
                        for (j = 0; j < n; ++j)
                            if (!visited[j]) { nxt = j; break; }
                    }
                }
                if (nxt < 0) break;
                ant_tours[a][step] = nxt;
                visited[nxt] = 1;
                cur = nxt;
            }
            ant_len[a] = mlab_tsp_length(inst, ant_tours[a]);
            ++n_tours;
            if (use_2opt)
                ant_len[a] = mlab_tsp_two_opt_refine(inst, ant_tours[a],
                                                     two_opt_pass, &n_2opt);
            if (ant_len[a] < iter_best) {
                iter_best = ant_len[a];
                iter_best_a = a;
            }
            if (ant_len[a] < best_len) {
                best_len = ant_len[a];
                memcpy(best_tour, ant_tours[a], (size_t)n * sizeof(int));
            }
        }

        /* 蒸发 */
        for (i = 0; i < n * n; ++i)
            tau[i] *= (1.0 - rho);

        /* 沉积 */
        if (mmas) {
            /* MMAS：仅全局最优回路沉积（每边 1/L_best），随后动态上下界钳制 */
            double dep = 1.0 / (best_len > 1e-12 ? best_len : 1.0);
            int *bt = best_tour;
            for (i = 0; i < n; ++i) {
                int a0 = bt[i], b0 = bt[(i + 1) % n];
                tau[a0 * n + b0] += dep;
                tau[b0 * n + a0] += dep;
            }
            {
                /* τmax=1/(ρ·L_best)：沉积-蒸发平衡点；τmin=τmax/(2n) 维持探索 */
                double tau_max = 1.0 / (rho * (best_len > 1e-12 ? best_len : 1e-12));
                double tau_min = tau_max / (2.0 * (double)n);
                for (i = 0; i < n * n; ++i) {
                    if (i / n == i % n) continue;
                    if (tau[i] > tau_max) tau[i] = tau_max;
                    if (tau[i] < tau_min) tau[i] = tau_min;
                }
            }
        } else if (elitist) {
            int *bt = best_tour;
            double dep = q_deposit / (best_len > 1e-12 ? best_len : 1.0);
            for (i = 0; i < n; ++i) {
                int a0 = bt[i], b0 = bt[(i + 1) % n];
                tau[a0 * n + b0] += dep;
                tau[b0 * n + a0] += dep;
            }
        } else {
            for (a = 0; a < nants; ++a) {
                double dep = q_deposit / (ant_len[a] > 1e-12 ? ant_len[a] : 1.0);
                int *t = ant_tours[a];
                for (i = 0; i < n; ++i) {
                    int a0 = t[i], b0 = t[(i + 1) % n];
                    tau[a0 * n + b0] += dep;
                    tau[b0 * n + a0] += dep;
                }
            }
            /* 迭代最优也略加强 */
            (void)iter_best_a;
        }
        /* 信息素下界，防止过早枯竭（轻量 MMAS） */
        {
            double tau_min = tau0 * 0.01;
            double tau_max = tau0 * 100.0;
            for (i = 0; i < n * n; ++i) {
                if (i / n == i % n) continue;
                if (tau[i] < tau_min) tau[i] = tau_min;
                if (tau[i] > tau_max) tau[i] = tau_max;
            }
        }
        out->iter_used = it + 1;
    }

    out->best_len = best_len;
    out->n_tours = n_tours;
    out->n_2opt_evals = n_2opt;

    for (a = 0; a < nants; ++a) free(ant_tours[a]);
    free(ant_tours);
    free(ant_len);
    free(tau);
    free(eta);
    free(visited);
    return 1;
}
