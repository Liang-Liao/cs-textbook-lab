# B2-newton-quasi-tr 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 全部实验确定性（谱构造 seed=2/3/17，噪声 seed=4242，起点 seed=1717），全量运行自动重算 `results/*.csv`
- run 历史缓冲须经 `mlab_opt_run_init` 挂接

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E1 | cg/finite_termination_spd | ≤ n 步残差 <1e-10（L216） | n=12：12 步，2.9e-18 | PASS |
| E1 | cg/zero_rhs_solution_zero | b=0 → 解 0 | ‖x‖=3.6e-16 | PASS |
| E2 | newton/rosenbrock_convergence_order | 牛顿阶 2±0.3（L217） | 2.022 | PASS |
| E2 | newton/quadratic_one_step_exact | SPD 二次一步精确 | iters=2，f=7.4e-31 | PASS |
| E2 | bfgs/rosenbrock_superlinear_order | BFGS 阶 >1.2（L217） | 1.351 | PASS |
| E3 | bfgs/rosenbrock_vs_gd_fevals | BFGS 评估数差距 ≥5×（L218，真实计数） | 125 vs 4721（37.8×） | PASS |
| — | lbfgs/rosenbrock_mem5 | L-BFGS mem=5 收敛 | f=2.9e-27，fe=152 | PASS |
| — | lbfgs/lbfgs_fewer_evals_than_gd | L-BFGS 评估数少于 GD | 149 vs 4721 | PASS |
| — | dfp/rosenbrock_converges | DFP 收敛 | f=2.1e-26，52 iters | PASS |
| E4 | trust_region/dogleg_fewer_failures_than_gd | 信赖域失败率低于线搜索，≥10 起点可复现（L219） | 12 起点：GD 12 败 vs Dogleg 0 败 | PASS |
| — | dogleg/classic_start_vs_gd | 经典起点对比 | dogleg f=1.1e-24 | PASS |
| — | dogleg/small_delta_recover | 极小初始半径恢复 | δ0=1e-3 → f=6.8e-29 | PASS |
| — | nelder_mead/sphere_dim4 | NM 收敛球面 | f=1.1e-10 | PASS |
| — | nelder_mead/rosenbrock_dim2 | NM 近似收敛 | err=3.2e-4 | PASS |
| E5 | nelder_mead/noise_robust_vs_gradient | NM 误差 ≤3×噪声幅，梯度法显著劣化（L220） | amp=0.05：NM 0.146 vs GD 3.77 | PASS |
| E6 | coord_descent/hooke_jeeves/direct 四 case | 无导数/全局方法收敛 | 全部收敛（DIRECT Rosenbrock 进入谷底 f=0.66<1） | PASS |
| — | bench/known_minima、conv/est_factor_geometric | 基准值与估计器 | 精确 | PASS |

## 总判定

- PASS: 21 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 修复与新增说明

- **修复（P1）**：收敛阶实测替换占位常数（原 `gnorm<1e-10 ? 2.0 : 0`）；GD/Newton 的 fevals 真实计数（原 `+= 3 / += 30` 伪造）——L218 判据建立在真实计数上；信赖域 vs GD 扩为 12 个病态起点；补 NM 噪声实验；Newton 无 Hessian fallback 改为真实的梯度中心差分；`mlab_cg_solve` 输出初始化；NM shrink 后更新 best-ever；删除 main 无条件预运行。
- **新增（路线图 L200-205 知识点落地）**：L-BFGS（双循环递推；曲率对 sy≤0 时清空缓冲退回最速下降，防冻结停滞）、DFP、坐标下降（黄金分割逐坐标精确极小化）、Hooke-Jeeves（探测+模式移动）、DIRECT（潜在最优盒下左凸包 + 全部最宽维同时三分，最宽维上限 3）。
- 收敛阶口径：f_hist 上以 `mlab_est_order_pair` 估计（f~Ce² 时 f-阶=误差-阶），取最后 3 个有效三元均值，避开非渐近段与噪声层。
- DIRECT 说明：Rosenbrock 上全局收敛到谷底附近（f=0.66），谷内精修需局部方法（M2 混合框架的动机）。
