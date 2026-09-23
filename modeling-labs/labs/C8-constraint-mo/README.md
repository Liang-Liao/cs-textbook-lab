# C8 约束处理与多目标优化

Deb 可行性法则 / 死亡惩罚 / 静态罚 / 自适应罚 / 修复法 DE；NSGA-II 与 MOEA/D（Tchebycheff）解 ZDT1；HV 与 IGD 排序翻转反例。

## 知识点

- EA 约束处理谱系：死亡惩罚、修复法、Deb 可行性法则、静态罚、自适应罚
  （Hadj-Alouane 风格按可行比例乘性调整 μ）
- 可行体积占比极小时死亡惩罚缺乏可行梯度信号（viol_min 全程不降）
- 静态罚 μ 敏感性 vs 无参数/自适应路线（与 B5 连续优化侧罚扫描对照）
- 修复算子（球约束投影）把不可行试验向量拉回可行域，挽救死亡惩罚
- Pareto 支配、非支配排序、拥挤距离、精英保留（NSGA-II）
- MOEA/D 分解法（Tchebycheff：g^te = max(λ1|f1−z1|, λ2|f2−z2|)、均匀权重、
  邻域交配 + δ 全局交配、理想点 z 动态更新；MOEA/D-DE 风格差分变异）
- ZDT1 真值前沿：f2 = 1-√f1
- IGD（到真值参考集的平均最近距离）与 2D HV（支配体积）的度量失真
- vendor 修复轮裁剪：仅保留 rng 链（dist/stats/gof），mc/bench/de 6 文件未用删除

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | Deb vs Death | 成功率差 **≥20pp**（100 次，稀疏可行域） |
| E2 | 违反度信号 | Deb 可行数更高 / 全程最小违反度 viol_min 更低 |
| E3 | 静态罚 μ 敏感性 | μ∈{1e-2..1e6} 成功率起伏 ≥20pp；Deb/自适应罚不低于最优静态档 |
| E4 | 修复法 | 修复算子使死亡惩罚成功率提升 ≥20pp 且 ≥80% |
| E5 | ZDT1 IGD（NSGA-II） | 最终 **IGD < 1e-2**，收敛衰减段可辨 |
| E6 | 端点覆盖 | 前沿端点与真值端点误差 **< 5%** |
| E7 | HV vs IGD | 构造近似集使 **排序翻转** |
| E8 | MOEA/D（Tchebycheff） | ZDT1 最终 IGD < 1e-2（≥4/5 seed），与 NSGA-II 对照落盘 |

路线图原文锚点：

- 可行性法则成功率比死亡惩罚高 ≥ 20 个百分点（100 次运行）
- ZDT1 上最终 IGD < 1e-2（给定预算内）且收敛曲线单调衰减段可辨
- 极端点覆盖：NSGA-II 前沿端点与真值前沿端点误差 < 5%
- 罚参数敏感性 vs 自适应（与 B5 实测对照：同一约束问题两条路线）
- MOEA/D 分解法概览（加权和 vs Tchebycheff；地图）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | cde（约束 DE：5 种约束处理模式）、nsga2、moead |
| `test/` | harness + test_main |
| `vendor/` | A2(dist/stats/gof) + C1(rng)（修复轮裁剪后 8 文件） |

测试 suite：cde、nsga2、moead、metrics

```bat
mingw32-make check-C8
bin\test.exe --list
bin\test.exe
bin\test.exe cde
bin\test.exe nsga2/zdt1_igd_below_1e2
mingw32-make -C labs/C8-constraint-mo check TEST_ARGS=<suite|suite/case>
```
