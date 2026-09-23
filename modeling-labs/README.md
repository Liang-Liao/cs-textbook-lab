# modeling-labs

数学建模与参数优化理论路线图的 **C 语言实验实现**。

理论地图：[`docs/数学建模与参数优化理论知识路线图.md`](docs/数学建模与参数优化理论知识路线图.md)  
总体设计：[`docs/compose/spec/modeling-labs-roadmap.md`](docs/compose/spec/modeling-labs-roadmap.md)

## 工具链

Windows + MSYS2 UCRT64，`gcc` + `mingw32-make`，C11，仅 `libm`。

## 构建与测试

```bat
mingw32-make check              & rem 全部 lab 全量测试
mingw32-make check-C1           & rem 指定 lab 全量
mingw32-make -C labs/C1-rng-mc check TEST_ARGS=rng
mingw32-make -C labs/C1-rng-mc test-list
```

lab 内测试入口是 `bin/test`（不是 check）：

```bat
cd labs\C1-rng-mc
bin\test.exe --list
bin\test.exe                 & rem 全算法功能测试
bin\test.exe rng             & rem 只测 rng 算法
bin\test.exe mc/gauss_box_convergence_slope
```

每个业务算法在测试里是独立 suite，并含多个场景用例（正常/边界/对照）。

## 代码组织（按 lab 自包含）

```text
labs/<lab-id>/
  src/     本章知识点算法（正本）
  test/    harness + test_main（CLI 选 suite/case）
  vendor/  前序 lab 业务算法 copy
  report.md / results/
```

- 本 lab 学什么，算法就在本 lab 的 `src/`
- 需要前面章节的算法时，从对应 lab 的 `src/` **copy** 到本 lab 的 `vendor/`
- **没有仓库级 `common/`**：共享算法以「首次教学 lab 的 src」为正本

## Lab 清单

| Lab | 章节 | src 业务算法 | 状态 |
|---|---|---|---|
| A1-numeric-la | 数值与线代 | linalg/numcal/vec/mat | 完成 |
| A2-prob-stat | 概率统计 | dist/stats/gof/rng | 完成 |
| A3-convex-opt | 凸分析 | conv | 完成 |
| B1-line-search-gd | 线搜索与 GD | linesearch/opt | 完成 |
| B2-newton-quasi-tr | 牛顿/CG/信赖域 | optcore/bench | 完成 |
| B3-least-squares | 最小二乘 | lsq | 完成 |
| B4-lp-ip | LP/IP | simplex/knapsack | 完成 |
| B5-constrained | 约束优化 | nlp/box_qp | 完成 |
| B6-sgd-adam | SGD/Adam | sgd | 完成 |
| B7-prox-admm | 近端/ADMM | prox | 完成 |
| C1-rng-mc | RNG 与 MC | rng/mc | 完成 |
| C2-mcmc | MCMC | mcmc | 完成 |
| C3-sa | 模拟退火 | sa/tsp | 完成 |
| C4-ts-vns-ils | TS/VNS/ILS | ts/vns/ils | 完成 |
| C5-ga-de | GA/DE | ga/de | 完成 |
| C6-es-cmaes | ES/CMA-ES | es/cmaes | 完成 |
| C7-pso-aco | PSO/ACO | pso/aco | 完成 |
| C8-constraint-mo | 约束/多目标 | cde/nsga2 | 完成 |
| C9-bayes-opt | 贝叶斯优化 | gp/bo | 完成 |
| D1-interp-spline | 插值样条 | interp | 完成 |
| D2-ode-sim | ODE 仿真 | ode | 完成 |
| D3-graph-combo | 图与组合 | graph/tsp | 完成 |
| M1-curve-fitting | 曲线拟合 | fitbench | 完成 |
| M2-global-bench | 全局基准 | gbench | 完成 |
| M3-e2e-param-est | 端到端参数估计 | e2e | 完成 |
