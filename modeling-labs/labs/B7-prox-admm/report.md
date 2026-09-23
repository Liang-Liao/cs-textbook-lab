# B7-prox-admm 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 全部实验固定 seed（21/33/55/77/99/123），确定性可复现

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| — | prox_op/soft_threshold_basic | 软阈值边界 | st(3,1)=2, st(0.5,1)=0 | PASS |
| — | prox_op/soft_threshold_monotone | 单调 + 收缩 | 2.5→2.00 | PASS |
| E1 | ista/sparse_support_recovery | 支持恢复 ≥90%（L339） | 100% | PASS |
| — | ista/zero_lambda_fits_noise_free | λ=0 退化为 LS | obj=9.9e-31 | PASS |
| E2 | fista/faster_than_ista_same_gap | FISTA ≤ ISTA/2（L340） | 162 vs 1863（11.5×） | PASS |
| E4 | subgradient/slower_than_ista_but_converges | 次梯度慢于 ISTA 但收敛（L327/334） | 50 步：ISTA 0.4010 < 次梯度 0.4022；6e4 步追近 | PASS |
| E6 | prox_point/matches_ista_objective | 近端点法（L328） | 相对目标差 0 | PASS |
| E5 | basis_pursuit/recovers_sparse_exact | 无噪 BP 精确恢复（L331） | resid=4.1e-28，max\|dw\|=1.4e-15 | PASS |
| — | admm/objective_matches_ista | ADMM-LASSO 一致 | rel diff=3.1e-8 | PASS |
| — | admm/rho_invariance_same_solution | ρ 不变性 | ρ=0.2 vs 8：max\|dw\|=8.9e-16 | PASS |
| E3 | admm/matches_auglag_on_eq_qp | ADMM vs B5 AL 带约束 QP 对账 <1e-8（L336/341） | 两法均 (0.5,0.5)，差=8.9e-16 | PASS |

## 总判定

- PASS: 11 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 修复与新增说明

- **修复**：① ADMM-LASSO 的 Cholesky 分解改为循环外一次分解复用（原每迭代重复分解同一矩阵）；分解失败显式返回 -1（目标非负）；② FISTA/ISTA 迭代数搜索由逐 k 重跑（O(K²)）改为单次运行顺序记录，效率大幅提升；③ 新增 BP/次梯度/近端点过程中修复一处缓冲区尺寸错误（Xᵀcoef 为 n 维却写入 m 维缓冲 → 堆破坏崩溃，gdb 回溯定位）。
- **新增**：次梯度法（次微分 + α0/√k 衰减，L327）；近端点法（近端子问题内层 ISTA，L328）；基追踪 BP-ADMM（Boyd 标准形式，x-update 为仿射集投影，L331）；`mlab_admm_qp_eq`（带等式约束 QP 的 ADMM，z-update 为仿射集投影）并与 vendor 的 B5 增广拉格朗日同题对账——对账差 8.9e-16 ≪ 路线图要求的 1e-8（L341）。
- vendor 链扩展：为对账引入 B1 linesearch、B2 optcore/bench、B5 nlp（sync_vendor 同步，逐字节一致）。
