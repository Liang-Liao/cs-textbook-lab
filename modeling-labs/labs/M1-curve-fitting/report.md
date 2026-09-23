# M1-curve-fitting 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — fitbench（双引擎拟合 + CI + σ 扫描 + 自动报告）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + A3(conv) + B1(opt/linesearch) + B2(optcore/bench) + B3(lsq) + C1(rng)
- seed: demo 90101；coverage 910k；domain 92k/93k；σ 扫描 940k/950k

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-pipeline | 合成→双引擎→报告落盘 | LM/BFGS 同解 th≈(1.806,0.656)，报告写入 `results/m1_demo_*.md` | PASS |
| E2-coverage | 500 次 95% CI 覆盖率 ∈ **[88%, 99%]**（t(28) 分位） | th0=**95.4%** th1=**94.4%** joint=**91.4%**，fail_fit=0 | PASS |
| E3-domain | 收敛域内 LM/BFGS 成功率 **100%**（真值 ±20% 初值，收敛+恢复双口径） | ±20% 初值 ×30：LM **30/30** BFGS **30/30**（收敛 30/30 且恢复 30/30） | PASS |
| E4-sigma | 5 档 σ 误差线性放大（斜率≈理论 1） | log-log slope a=**0.935** b=**0.848**；err 比≈**9.9**（σ 比 16） | PASS |
| E5-theory | mean\|err\|/se ≈ 高斯常数 | **0.823**（理论 √(2/π)≈0.798） | PASS |

## 关键数据

- 覆盖率逐次: `results/m1_coverage_trials.csv`
- 收敛域成功率: `results/m1_domain_success.csv`
- σ 扫描: `results/m1_sigma_scan.csv`
- 演示报告: `results/m1_demo_lm.md`、`results/m1_demo_bfgs.md`

## 结论与备注

- **工作台流水线**：`fitbench` 提供合成数据生成、LM（B3）与 BFGS（B2 optcore）双引擎、FD 雅可比协方差、95% CI、残差/参数/协方差自动报告。
- **CI 口径升级（R7）**：置信区间改用 Student-t 分位 `t(m-npar)=t(28)≈2.0484`（`mlab_fit_t_crit95`，经不完全 Beta 函数数值求逆实现，与标准表误差 <5e-5），不再用正态 1.96。t 分位下 500 次重复两参数边际覆盖率 95.4%/94.4%（1.96 口径下 94.6%/92.8%），更接近名义 95%，联合率 91.4%。
- **成功率双口径（R7）**：初值扰动改真 ±20%（此前实际 ±10%）；成功 = 引擎收敛（status==0）**且** 参数恢复（相对误差 <10%），不再忽略 status。LM tol 1e-10、BFGS tol 1e-8 下双口径均 30/30。
- **收敛阈值的数值依据（R7）**：本拟合目标 f=0.5·RSS≈0.024，双精度对 RSS 的分辨极限使梯度范数可达下限约 √(ε·f·λ)≈1e-8，此前 BFGS tol=1e-10 会因线搜索无法继续改进而假性不收敛；LM 侧 B3 R3 起“内层停滞”返回码 2 区分后，tol=1e-12 的相对 RSS 改善也无法可靠触发。两处测试阈值分别回调至 1e-10/1e-8（仍比统计精度 σ/√m≈0.015 严格 7 个量级以上），参数恢复不受影响。
- **fevals 去双计（R7）**：`mlab_fit_bfgs` 此前 `run.fevals + ctx.fevals` 把同一批目标求值计了两次（optcore run 已逐次计数），现直接用 `run.fevals`。
- **σ 线性放大**：恢复误差随噪声近似线性增长（log-log 斜率约 0.85–0.94，接近理论 1）；mean|err|/se≈0.82 与高斯 E\|N\|/σ 常数吻合，说明协方差估计口径正确。
- 注记：主实验模型为指数衰减；exp-sin 四参数模型已预留接口但覆盖率主路径用二参数模型以控制 500 次成本。
- common 增量: `mlab_fit_*`（synth / LM / BFGS / cov / CI / t 分位 / report）。

## 总判定

- PASS: 5 / FAIL: 0 — **PASS**
