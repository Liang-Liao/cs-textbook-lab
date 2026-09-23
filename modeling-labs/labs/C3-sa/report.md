# C3-sa 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — sa（Metropolis/几何+对数调度/重加热/多重启/T0 标定）+ tsp（swap/2-opt SA，恒等移动跳过）+ bench_sa（Rastrigin）
- vendor: A2(dist/stats/gof) + C1(rng/mc) + B1(opt.h)（mcmc 冗余已清理）
- seed: hit=73001+, scan=81000+, repro=91001/92003/93001/94003, accept=77001, tsp=33001/36000+/38000+

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-rastrigin-hit | 调参后 1000 次 2D Rastrigin 全局命中率 ≥ 80% | **0.969**；参数 α=0.99, inner=40, step=0.20, T0=1.0；平均 fe≈36680 | PASS |
| E2-param-grid | 热图覆盖扫描网格；同参复跑 \|Δ\|<5pp | 48/48 格（4α×3inner×4T0）；tuned \|0.955-0.955\|=0；mid \|0.530-0.525\|=0.005 | PASS |
| E3-tsp-2opt | 2-opt 最终回路显著优于 swap，配对 p<0.01 | swap=32.62, 2opt=31.61, Δ=1.01, **p=8.0e-4** | PASS |
| E4-accept-curve | 接受率随温度下降；有效降温窗口 | acc0=0.50→accN=0.00，corr(T,acc)=0.92，window=12 层 | PASS |
| E5-t0-calibration | 目标接受率反推 T0 | target 0.8: T0≈52.4 acc=0.823；target 0.3: T0≈9.1 acc=0.410——标定旋钮有效 | PASS |
| E6-reheat-restart | 重加热/多重启（短预算 4.6k 评估/跑） | single=0.590，multirestart8=**1.000**（+41pp>15pp）；reheat 周期回 0.5·T0 命中 0 但 mean_fbest=0.05（vs log 1.82） | PASS |
| 对照 | SA-2opt vs 恒接受随机行走（双方跳过恒等移动） | sa=33.89 vs rw=44.12，ratio=0.768 | PASS |

## 关键数据

- 命中率/复跑: `results/sa_hit_rate.csv`
- 参数网格: `results/sa_param_scan.csv` + `results/sa_param_scan.pgm`（亮度=命中率）
- 接受率轨迹: `results/sa_accept_curve.csv`
- T0 标定: `results/sa_t0_calibration.csv`
- 重加热/多重启: `results/sa_reheat_restart.csv`
- 参数网格补列: `results/sa_param_scan.csv`（新增 avg_fevals 平均迭代数列）
- TSP: `results/tsp_instance.csv`、`results/tsp_swap_vs_2opt.csv`

## 结论与备注

- **调参主组**: α=0.99, inner=40, step_scale=0.20（相对盒边长）, T0=1.0, T_min=1e-4 → 命中率 96.9%。
- 敏感性: 低 inner/低 α/过小 step 命中率明显下降（见 PGM）；同参数复跑波动远小于 ±5pp。
- **步长与温度耦合**: 优化用 σ∝T（探索→收敛）；接受率诊断必须用**固定步长**，否则低温步长过小导致近似贪心全接受，曲线回升。
- TSP: 同预算下 2-opt 平均更短且配对显著；多数对二者收敛到同一局部回路，swap 更常卡在更差回路。
- 重加热/多重启已落地：短预算（4.6k 评估/跑）下多重启 best-of-8 命中率 100% vs 单跑 59%——拼预算的务实正解；
  周期重加热保持流动性（mean_fbest 0.05 优于对数调度 1.82），但温度不再降至 T_min，精确命中（f<1e-6）归零——
  重加热适合"逃离+再收敛"策略，需保证重冷段层厚（本例需 ≥590 层）。
- 对数调度 T=T0·ln2/ln(1+k) 已实现：同预算下前 30% 层即降至 0.3·T0，之后长期徘徊在 0.1~0.2·T0，
  实测命中率 0（冻结不到 T_min）——印证"理论收敛保证但实用慢"的经典结论。
- 2-opt SA 提议跳过恒等（hi==lo+1）与等长整段逆向（0,n-1）移动，随机行走基线同口径，预算对齐。
- T0 标定目标 0.8 时 T0≈52（Rastrigin 上坡 Δf 均值大），首层接受率 0.823 达标；标定旋钮（0.8→0.3）单调有效。
- common 增量: `mlab_sa`（多调度/重加热/多重启/T0 标定）、`mlab_tsp`、`mlab_rastrigin`。

## 总判定

- PASS: 7 / FAIL: 0 — **PASS**
