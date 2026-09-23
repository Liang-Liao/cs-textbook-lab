# C7 群智能：粒子群与蚁群

PSO（惯性 w、星型/环型拓扑、认知/社会消融）+ ACO（TSP 信息素、ρ 扫描、2-opt 精修）与 SA/贪心对账。

## 知识点

- PSO 更新：`v ← w v + c1 r1 (pbest-x) + c2 r2 (nbest-x)`；三分量消融
- 惯性权重 w（固定 / 线性递减）；大 w 利探索
- 拓扑：全局星型 / 环型 / Von Neumann（二维网格 4 邻居）
- Clerc 收缩因子：`v ← χ(v + c1 r1 … + c2 r2 …)`，χ=0.729、c1=c2=1.4962，无需 vmax 钳制的收敛保证
- 边界：速度钳制 + 位置吸收
- ACO：`p(j) ∝ τ^α η^β`，蒸发 ρ，精英沉积 / AS 全体沉积；MMAS 动态上下界
  （τmax=1/(ρ·L_best) 随最优长度收紧，τmin=τmax/2n 保底探索）；2-opt 精修
- 评估口径：n_tours（构造回路数）与 n_2opt_evals（2-opt 长度评估数）分列
- 注记：新元启发式与 PSO/GA 同构

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | PSO 基线 | Sphere/Rosenbrock/Rastrigin 收敛数据落盘 |
| E2 | w×拓扑热图 | ≥7×2 组合，每格 ≥100 次；多峰最优 w 相对单峰**右移** |
| E3 | 消融 | 完整版在 Rastrigin 上成功率显著高于认知-only / 社会-only（p<0.01） |
| E4 | ACO ρ 扫描 | n=40 TSP（关 2-opt、AS 沉积、τ² 主导），多档 ρ 全表落盘且最优/最差档区分度 ≥5% |
| E5 | ACO vs SA | ACO+2opt 优于纯 SA（p<0.05），相对贪心 gap ≤5% |
| E6 | Clerc + Von Neumann | Clerc 收缩无 vmax 在 Sphere 收敛可靠（≥90%）；VN 拓扑正常工作并落盘三拓扑对照 |
| E7 | MMAS 动态上下界 | 同实例同 seed 下 MMAS 显著优于精英 AS（p<0.05） |

路线图原文锚点：

- w×拓扑成功率热图完整（≥ 7×2 组合，每格 ≥ 100 次运行），"大 w 利探索"表现为最优 w 区间随问题多峰性右移
- 消融实验：完整版在多峰问题上成功率显著高于任一单项版（p < 0.01）
- ACO（+2-opt 精修）在 n=30 TSP 上解质量优于纯 SA（配对检验 p < 0.05）且相对贪心基线超出 ≤ 5%

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | pso、aco（含贪心 NN + 2-opt 精修） |
| `test/` | harness + test_main |
| `vendor/` | A2 + C1 + B2(bench) + C3(bench_sa/tsp/sa) |

测试 suite：pso、aco

```bat
mingw32-make check-C7
bin\test.exe --list
bin\test.exe
bin\test.exe pso
bin\test.exe aco/aco2opt_beats_sa_near_greedy
mingw32-make -C labs/C7-pso-aco check TEST_ARGS=<suite|suite/case>
```
