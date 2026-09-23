# C8-constraint-mo 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — cde（死亡惩罚/Deb/静态罚/自适应罚/修复法 DE）、nsga2、moead（Tchebycheff 分解）
- vendor: A2(dist/stats/gof) + C1(rng)——修复轮裁剪后 8 文件（mc/bench/de 6 文件未用删除）
- seed: CDE 81k–83k / 86k–87k；NSGA-II 85001；MOEA/D 88k；度量反例为构造目标向量

## 本轮口径更新

- **cde 评估口径**：父代只在初始代整群评估一次、由选择步维护（与 C5 de 对齐），
  末代代表解改由缓存数组读取（旧实现在无可行解时重新评估整个种群且未计数）。
- **违反度信号口径**：新增 `viol_min`（全程最小违反度）；死亡惩罚的对照信号改用
  最小违反度（旧口径实为"第一个体的违反度"，无意义）。
- **死代码清理**：删除评估循环中 Deb 分支的空 if 块与无效 best 跟踪。

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-cde-deb | Deb 成功率比死亡惩罚高 ≥20pp（100 次） | 稀疏可行域（r=0.45）：Deb **100/100** vs Death **0/100**，gap=**100pp** | PASS |
| E2-cde-signal | 违反度信号（DEATH 记全程最小值 viol_min） | Deb viol_min=0（40/40 可行）；Death viol_min=**1.77**（0/40 可行）——死亡惩罚无梯度信号 | PASS |
| E3-cde-mu | 静态罚 μ 敏感性 vs Deb/自适应罚（:539，B5 对照） | μ=1e-2/1e0/1e2/1e4/1e6 → 成功 0/0/40/40/40（起伏 40pp，欠罚失败）；Deb=40、自适应罚=40（无参数稳定） | PASS |
| E4-cde-repair | 修复法（:538） | 死亡惩罚+球面投影修复：成功率 **0% → 100%**（p=1e-12） | PASS |
| E5-zdt1-igd | 最终 IGD<1e-2，衰减可辨 | IGD g0=2.20 → mid=7.5e-2 → **end=9.91e-3**，Spearman=−0.999 | PASS |
| E6-zdt1-ends | 端点误差 <5% | (min f1)=(0,1.010) err=**0.010**；(max f1)=(1,0.006) err=**0.006** | PASS |
| E7-hv-igd | HV 与 IGD 排序翻转 | A_corner HV=1.19 IGD=0.68；B_true_pf HV=0.87 IGD=0.0075 → **HV 选 A，IGD 选 B** | PASS |
| E8-moead | MOEA/D（Tchebycheff，:544） | ZDT1 pop=60/T=12/gen=300（≈18k 评估）：IGD end=**7.54e-3**（5/5 seed <1e-2）；NSGA-II 对照 9.43e-3 | PASS |

## 关键数据

- 约束 DE: `results/cde_deb_vs_death.csv`、`results/cde_violation_signal.csv`（含 viol_min）、
  `results/cde_penalty_sensitivity.csv`（新增）、`results/cde_repair_death.csv`（新增）
- NSGA-II: `results/nsga2_zdt1_igd.csv`、`results/nsga2_zdt1_endpoints.csv`
- MOEA/D: `results/moead_zdt1_igd.csv`（新增，含 seed0 逐代收敛曲线与 NSGA-II 对照）
- 度量反例: `results/hv_igd_inversion.csv`

## 结论与备注

- **Deb vs 死亡惩罚**: 在可行体积占比极小的球约束问题上，真正的死亡惩罚（不可行适应度为同一常数）**完全没有**违反度梯度——全程最小违反度 viol_min≈1.77 不降、100 次全失败；Deb 法则 100% 找到可行近优解。
- **罚参数敏感性（与 B5 对照）**: B5 连续优化侧实测静态罚对 μ 敏感；本 lab 在同型球约束问题上用 DE 种群复现该结论——欠罚（μ≤1）时种群收敛到不可行最优（0/40），μ≥1e2 后全成功。Deb 法则与自适应罚（Hadj-Alouane 乘性调整）在**无手工调参**下达到最优静态档水平：EA 侧的两条无参数路线消除了 μ 敏感性。
- **修复法**: 把球面投影作为修复算子接入 DE 后，死亡惩罚从 0% 恢复到 100%——修复法把"约束满足"从选择压力转移到算子本身，是对死亡惩罚最直接的补救。
- **NSGA-II / ZDT1**: 环境选择为快速非支配排序 + 层内拥挤距离 + 精英合并。变异算子在 SBX 之外引入 DE/rand/1/bin（C5 差分机制），否则 g(x) 难以从 ~5 压到 1。IGD 单调衰减至 <1e-2，端点误差约 1%。
- **MOEA/D（Tchebycheff）**: 均匀权重 + 邻域交配（δ=0.1 全局交配）+ 理想点动态更新；子代用 MOEA/D-DE 风格差分变异（纯 SBX/PM 逐维研磨 g 过慢，实测 15k 评估 IGD 仅 ~0.9）。差分变异后 18k 评估 IGD 7.5e-3，与 NSGA-II 同量级。原种群中边界子问题（如 λ=(1,0)）的解 f2 很差，属分解法固有形态（各子问题只优化自己的标量化目标），判据口径与 NSGA-II 一致用种群整体对参考集的 IGD。
- **HV vs IGD**: 角点可获得大支配体积（HV 高）却 IGD 很差；均匀贴合真值前沿的点集相反。单指标会误导算法排序。
- 未实现：精确超体积积分（本 lab 2D 用扫描 HV）、MOEA/D 加权和变体对照、Tchebycheff 归一化。
- common 增量: `mlab_cde_*`（含静态罚/自适应罚/修复模式）、`mlab_deb_better`、`mlab_nsga2_*`、`mlab_moead_*`、`mlab_zdt1_*`、`mlab_igd`、`mlab_hv2d`。

## 总判定

- PASS: 8 / FAIL: 0 — **PASS**
