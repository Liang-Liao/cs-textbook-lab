# A1-numeric-la 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量）/ `bin/test.exe <suite>` / `bin/test.exe <suite>/<case>`
- 全量运行时自动重算 `results/*.csv`（无随机量，确定性可复现）
- 本报告为手工维护版本（测试程序不再覆写 report.md）

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E1 | linalg/lu_well_conditioned | ‖Ax−b‖/‖b‖ < 1e-12×κ | κ=1：1.086e-15 < 1e-12 | PASS |
| E1 | linalg/lu_moderate_condition | 同上 | κ=1e4：1.172e-15 < 1e-8 | PASS |
| E1 | linalg/lu_ill_conditioned | 同上 | κ=1e6：1.083e-15 < 1e-6 | PASS |
| E1 | linalg/lu_singular_rejected | 奇异拒绝 | rc=-1 | PASS |
| E4 | linalg/cholesky_spd_reconstruct | ‖LLᵀ−A‖_F/n 极小 | 4.310e-16 | PASS |
| E4 | linalg/cholesky_non_spd_rejected | 非 SPD 拒绝 | rc=-1 | PASS |
| — | linalg/cond2_estimate_spd | 条件数估计量级正确（κ_true=1e4） | spd=1.000e4，general=1.000e4 | PASS |
| — | linalg/cond2_singular_rejected | 奇异返回错误 | -1 | PASS |
| E2 | numcal/fd_forward_u_shape | U 形，h*/√ε ∈ [0.3,3]（路线图 L114 原文区间） | h*=1.778e-8，ratio=1.193 | PASS |
| E2 | numcal/fd_central_u_shape | U 形，h*~ε^{1/3} | ratio=0.522，err=1.66e-12 | PASS |
| E2 | numcal/fd_forward_vs_central | 同 h 中心差分更准 | 1.21e-11 < 5.0e-6 | PASS |
| E2 | numcal/forward3_second_order | 三点前向 O(h²) 优于一点 O(h) | 3.595e-3 < 5e-3 且 < 0.25×5.17e-2 | PASS |
| E3 | numcal/simpson_convergence_order | 斜率 = 4 ± 0.3（路线图 L115 原文） | −3.9973 | PASS |
| E3 | numcal/trapezoid_vs_simpson_smooth | Simpson 更准 | 1.456e-7 < 5.59e-4 | PASS |
| E3 | numcal/simpson_sin_interval | ∫sin[0,π]=2 | 误差 2.52e-10 | PASS |
| E5 | numiter/jacobi_diag_dominant_converges | 对角占优收敛 | 34 iters，res=1.76e-12 | PASS |
| E5 | numiter/gs_faster_than_jacobi | ρ_GS=ρ_J² → GS≈一半 | 1073 vs 2082 | PASS |
| E5 | numiter/sor_optimal_beats_gs | 最优 ω SOR 显著快 | ω*=1.7406：105 vs 1073 | PASS |
| E5 | numiter/sor_omega1_matches_gs | ω=1 退化为 GS | 1073 == 1073 | PASS |
| E5 | numiter/jacobi_divergence_detected | ρ>1 发散被检出 | ρ=2.0000，rc=1 | PASS |
| E5 | numiter/jacobi_rho_matches_theory | Poisson ρ=cos(π/(n+1)) | 0.98883083（理论 0.98883083） | PASS |

## 总判定

- PASS: 24 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 备注

- results 数据说明：`fd_u_shape.csv` 的 h 网格为 10^(-16+0.75i)（与现行 `fd_scan` 一致）；`lu_residual.csv` 阈值列为 1e-12×κ 公式值；`numiter_iters.csv` 为 Poisson20 四种方法对比。
- 条件数估计为幂迭代近似（200 次迭代），在谱可控矩阵上与真值一致到显示精度。
