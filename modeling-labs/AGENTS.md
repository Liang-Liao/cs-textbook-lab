# modeling-labs Agent 约定

本仓库按 `docs/数学建模与参数优化理论知识路线图.md` 逐节实现 C 语言实验 lab。

## 硬性约定

- 语言：C11；零第三方数值库；仅链接系统 `libm`
- 工具链：MSYS2 UCRT64，`gcc` + `mingw32-make`

## 目录组织（学习边界 = 单个 lab 目录）

```text
labs/<lab-id>/
  src/        本 lab 知识点对应的业务算法实现（要读的主体）
  test/       测试：harness + test_main.c（不再用 check.c 命名）
  vendor/     前序 lab 业务算法的 copy（编译用这份；只读）
  README.md   知识点、实验清单、判据
  report.md   自检报告
  results/    实验数据
```

规则：

1. **每个 lab 的知识点算法写在自己的 `src/`**
2. **测试在 `test/`**：`harness.c/h` + `test_main.c`；每个业务算法一个 **suite**，每个算法下多个 **场景 case**
3. **CLI 可选全量 / 单算法 / 单场景**：
   ```bat
   bin\test.exe                 # 全量功能测试
   bin\test.exe --list          # 列出 suite/场景
   bin\test.exe linalg          # 只测某一算法
   bin\test.exe linalg/lu_well_conditioned
   mingw32-make check TEST_ARGS=rng
   mingw32-make check TEST_ARGS=mc/hitmiss_pi_n20000
   ```
4. **若依赖前面 lab 的业务算法，copy 到本 lab 的 `vendor/`**
5. **无 `common/` 目录**：算法正本只在各 lab 的 `src/`
6. 随机实验固定 seed；报告中文
7. 提交：`src/`、`test/`、`vendor/`、`report.md`、`results/`
8. `mingw32-make check-<id>` 全 PASS 后一次 commit

## 测试约定

- suite 名 = 算法/模块名（如 `linalg`、`ista`、`mc`）
- case 名描述**场景**（如 `lu_ill_conditioned`、`cholesky_non_spd_rejected`）
- 每个业务算法至少 2 个不同场景（正常路径 + 边界/失败/对照）
- case 通过 `test_record(ok, suite, name, detail)` 记录

## 算法归属（src 正本）

| Lab | src/ 中的业务算法（知识点） |
|---|---|
| A1 | vec/mat/linalg（LU/Cholesky/谱构造）、numcal（差分/Simpson） |
| A2 | dist、stats、gof（KS/卡方）、rng（采样工具） |
| A3 | conv（收敛因子/阶估计） |
| B1 | opt（问题接口）、linesearch（黄金分割/Armijo/最速下降） |
| B2 | optcore（Newton/BFGS/CG/Dogleg/NM）、bench（Rosenbrock 等） |
| B3 | lsq（正规方程/QR/LM/协方差） |
| B4 | simplex、knapsack |
| B5 | nlp（罚/AL/投影梯度）、box_qp |
| B6 | sgd |
| B7 | prox（ISTA/FISTA/ADMM） |
| C1 | rng（多发生器/质量/正态）、mc（积分/拒绝/IS） |
| C2 | mcmc（MH/Gibbs/ACT/R̂ 诊断） |
| C3 | sa（Metropolis/调度/T0 标定）、tsp（swap/2-opt 邻域）、bench_sa |
| C4 | tabu（禁忌表/任期/特赦）、vns_ils（VNS/ILS 扰动） |
| C5 | ga、de |
| C6 | es（(1+1)-ES/1/5 规则）、cmaes（协方差自适应） |
| C7 | pso（拓扑/消融/w）、aco（TSP 信息素 + 2-opt） |
| C8 | cde（Deb 可行性 vs 死亡惩罚 DE）、nsga2（NSGA-II + IGD/HV） |
| C9 | gp（RBF 高斯过程 + Cholesky）、bo（EI/PI/UCB、LHS、BO 循环） |
| D1 | interp（Lagrange/Newton、分段/Hermite、自然三次样条、RBF） |
| D2 | ode（Euler/RK4/隐式 Euler/RKF45、仿真模板、FD 敏感性） |
| D3 | graph（Dijkstra/Floyd/BF、MST、拓扑）、tsp（NN/2-opt/Held-Karp） |
| M1 | fitbench（双引擎拟合工作台、CI、σ 扫描报告） |
| M2 | gbench（统一基准、算法适配、实数 GA、混合框架） |
| M3 | e2e（LV/SEIR 仿真、混合点估计、MCMC CrI、覆盖率与报告） |

后续 lab 需要上表算法时：从对应 lab 的 `src/` **copy** 到自己的 `vendor/`（见各 lab `vendor/VENDOR.md`）。

## 新 lab 流程

1. 在本 lab `src/` 实现本章算法
2. 需要前序算法 → copy 进 `vendor/`，更新 `vendor/VENDOR.md`
3. `test/test_main.c` + `test/harness.*`：按算法建 suite，多场景 case
4. `mk/lab.mk` 自动 `-Isrc -Ivendor -Itest`，产出 `bin/test`
5. `mingw32-make check-<id>`（全量）→ report → commit

## 提交前检查

1. README 含知识点、实验、路线图判据原文，以及 `bin/test` 用法
2. `src/` 有本章算法源文件（不只有测试）
3. `test/test_main.c` 存在；`bin/test` 退出码 0；`--list` 可列出 suite/场景
4. `vendor/` 若有依赖则含 VENDOR.md 与 copy 文件
5. report.md 含环境、实测、PASS 表

## Commit 消息

```text
lab(<id>): <一句话>

- src: ...
- vendor: 来自 <前序 lab>/src: ...
- test: ...
- 判据: <关键锚点>
```

## 设计文档

`docs/compose/spec/modeling-labs-roadmap.md`
