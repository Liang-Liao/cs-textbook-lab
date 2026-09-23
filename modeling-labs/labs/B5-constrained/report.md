# B5-constrained 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 随机实验固定 seed（箱式问题 seed=9），全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E2 | penalty/large_mu_near_active_bound | 真 KKT 残差 <1e-6（路线图 L288） | x=1.0000，kkt=6.05e-09 | PASS |
| E1 | aug_lag/al_needs_1pct_of_penalty_mu | AL 达同精度所需 σ ≤ 纯罚 μ 的 1%（L287） | ε=1e-4：μ_min=1e4，σ_min=1（0.01%） | PASS |
| — | aug_lag/equality_constraint_multiplier | 等式约束乘子恢复 | ν=-0.5000（解析 -0.5），kkt=8.3e-12 | PASS |
| — | proj_grad/box_clamp_unconstrained_outside | 盒外投影 | x=(1,1)，kkt=0 | PASS |
| E3 | proj_grad/agrees_active_set_100_random | 100 随机箱式 QP 与有效集枚举一致（L288） | 100/100，mismatch=0 | PASS |
| — | box_qp/active_set_hits_upper_bounds | 有效集法 | x=(1,1) | PASS |
| — | box_qp/interior_solution | 盒内最优 | x=(-0.5,0.5) | PASS |
| E4 | interior/log_barrier_converges | 对数障碍内点法（L277） | x=0.999999，kkt≈e-8 | PASS |
| E4 | sqp/equality_qp_kkt | 最小 SQP 等式 QP（L279） | x=(0.5,0.5)，kkt=8.0e-11 | PASS |
| E4 | sqp/inequality_active_set | 最小 SQP 不等式 | x=1.000000，kkt=0 | PASS |

## 总判定

- PASS: 10 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 修复与新增说明

- **修复（P1）**：① `kkt_measure` 此前为 ‖∇f‖+违反度，缺乘子项，在约束最优处 ∇f≠0 时 1e-6 判据不可测——现为真 KKT 残差（罚导出乘子 λ=2μ·max(0,g) / AL 乘子 λ、ν），约束梯度用中心差分；② 投影梯度固定步长 0.05 无保护（L>40 即发散，且测试曾绕开 src 正本）——改为投影 Armijo 回溯（Bertsekas 口径），随机测试改调 src 正本；③ 罚目标梯度由整体前向差分（误差 ~1e-6）改为"基础目标解析梯度 + 约束中心差分"（~1e-9）。
- **新增**：对数障碍内点法（λ=μ/(−g)，μ 序列止于 1e-6 以免乘子估计被数值噪声淹没；度量用原问题互补松弛 |λ(−g)|）；最小 SQP（KKT 线性系统 [I Gᵀ;G 0] 的有效集 QP 子问题 + merit 回溯；修复中补上了缺失的 Gᵀ 转置块）。
- E1 口径：目标精度 ε=1e-4；纯罚 μ 从 1 扫到 1e6 得 μ_min=1e4（err≈1/(2μ)），AL σ 扫描得 σ_min=1——比值 0.01%，与理论（AL 乘子更新免于病态化）一致。
