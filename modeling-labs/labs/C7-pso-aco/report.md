# C7-pso-aco 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — pso（w/拓扑/消融/Clerc 收缩/Von Neumann）、aco（信息素/ρ/MMAS/2-opt）+ 贪心 NN
- vendor: A2(dist/stats/gof) + C1(rng/mc) + B2(bench) + C3(bench_sa/tsp/sa)
- seed: PSO 基线 61k；热图 70k；Clerc/VN 71k；消融 74k；ACO ρ 76k；MMAS 60k；对账 78k；实例 55001

## 本轮重设计

- **ρ 扫描重设计**：原设定（elitist 沉积 + β=3 + 2-opt 精修）下所有 ρ 档被拉平
  （2-opt 把每只蚂蚁的构造结果都精修到同一局部最优，mean_len 全档 ≈44.35）。
  重设计为：关 2-opt、AS 全体沉积（elitist 沉积相对蒸发过强）、τ² 主导
  （α=2, β=1：启发式项不再淹没信息素记忆）、n=40 城市 × 200 代——ρ 档间
  区分度 8.3%。
- **评估口径拆分**：`n_tours` 只计构造回路数；2-opt 精修的长度评估单列
  `n_2opt_evals`（此前混在同一计数器中）。

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-pso-baseline | 三函数收敛数据落盘 | mean best Sphere=6.6e-6，Rosenbrock=1.58，Rastrigin=2.09 | PASS |
| E2-pso-heatmap | w×拓扑 ≥7×2，每格≥100；多峰最优 w 右移 | **7×2×100**；mean-best 最优 w：Sphere **0.30** → Rastrigin **0.40**；成功率 top2 w 0.35→0.40；非零成功率最高 w 下标 3→4 | PASS |
| E3-pso-ablation | 完整版成功率显著高于单项（p<0.01） | 角落初始化 Rastrigin5D：hit full=**20** cog=**0** soc=**8**/100；p=1.2e-6 / 7.2e-3 | PASS |
| E4-aco-rho | ρ 扫描全表 + 区分度 | 6 档 ρ×8 次，n=40，无 2-opt：best ρ=**0.30** mean_len=**58.99**，worst=63.89，**spread 8.3%** | PASS |
| E5-aco-sa | ACO+2opt 优于 SA（p<0.05），贪心 gap≤5% | ACO+2opt=**44.35** < SA+2opt=**45.55**，p=1.4e-7；greedy=50.57，gap=**-12.3%**（优于贪心） | PASS |
| E6-pso-clerc-vn | Clerc 收缩因子 / Von Neumann 拓扑 | Clerc 无 vmax 钳制 Sphere10D 成功 **50/50**；Rastrigin mean：global=12.78 / ring=11.06 / **von_neumann=8.83** | PASS |
| E7-aco-mmas | MMAS 动态上下界 vs 精英 AS | 同实例同 seed 配对 n=30 TSP：AS=44.518 vs **MMAS=44.381**，p=3.8e-3 | PASS |

## 关键数据

- PSO: `results/pso_baseline_convergence.csv`、`results/pso_w_topo_heatmap.csv`、`results/pso_clerc_vonneumann.csv`（新增）
- 热图 PGM: `results/pso_w_topo_sphere.pgm`、`results/pso_w_topo_rastrigin.pgm`
- 消融: `results/pso_ablation_rastrigin.csv`
- ACO: `results/aco_rho_scan.csv`（n_2opt_evals 口径列）、`results/aco_mmas_vs_as.csv`（新增）、`results/aco_sa_greedy_tsp30.csv`

## 结论与备注

- **w 与探索**: 单峰 Sphere 上 mean-best 最优 w 偏小（0.30，利用即可收敛）；多峰 Rastrigin 最优 w 右移到 0.40——更大惯性才利于跨盆地探索。成功率热图显示过大 w（≥0.7）两问题都变差，存在探索-收敛权衡窗口。
- **拓扑**: 热图上环型在相同 w 下成功率通常低于全局星型（信息传播慢）；Clerc 参数化下三拓扑对照显示 **Von Neumann 在多峰 Rastrigin 上最优**（8.83 vs global 12.78）——4 邻居局部共享兼具传播速度与早熟抑制，与文献「lbest 类拓扑利于多峰」一致。
- **Clerc 收缩因子**: χ=0.729、c1=c2=1.4962 的经典参数化在**完全不使用 vmax 钳制**时于 Sphere10D 成功率 50/50——速度自限，收敛性保证实测成立。
- **消融**: 随机全域初始化时 social-only 常与完整版打平；**角落初始化**后 social-only 锁死邻近局部最优（hit 8），认知-only 无信息共享（hit 0），完整版显著更高（hit 20）——分量归因落到可复现设定上。
- **ρ 敏感性**: 区分度需要「信息素记忆真的主导构造」——elitist 强沉积或 β=3 启发式主导时 ρ 失效（被拉平）。τ²+AS+无 2-opt 下最优 ρ=0.30 为内部值：过小 ρ 早熟停滞、过大 ρ 记忆被冲刷。
- **MMAS**: 动态上下界（τmax=1/(ρ·L_best) 收紧 + τmin=τmax/2n 保底）相对固定宽松界一致小幅改善（44.52→44.38，p=3.8e-3），量级与文献报告的同规模实例一致。
- **ACO vs SA**: 同一 n=30 欧氏实例上，ACO+2-opt（精英沉积 + ρ=0.3）配对显著优于 C3 纯 SA+2-opt，且相对多起点贪心基线更好（gap 为负）。
- ACO-SA 混合、RESTARS 为注记项。
- common 增量: `mlab_pso_*`（含 constriction/Von Neumann）、`mlab_aco_*`（含 MMAS）、`mlab_tsp_greedy_nn`、`mlab_tsp_two_opt_refine`。

## 总判定

- PASS: 7 / FAIL: 0 — **PASS**
