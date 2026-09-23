# A3-convex-opt 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 全部实验确定性（seed 固定：谱构造 seed=11，多起点 seed=42），全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E1 | conv/sd_factor_kappa_10 | 实测收敛因子与 (κ−1)/(κ+1) 相对误差 <20%（误差范数口径，路线图 L162） | emp=0.8180 vs 0.8182，rel=0.0%（125 iters） | PASS |
| E1 | conv/sd_factor_kappa_50 | 同上 | emp=0.9607 vs 0.9608，rel=0.0%（627 iters） | PASS |
| E1 | conv/sd_factor_kappa_200 | 同上 | emp=0.9900 vs 0.9900，rel=0.0%（2507 iters） | PASS |
| — | conv/order_pair_quadratic_sequence | 二次序列上阶估计 p=2 | p=2.0000 | PASS |
| E2 | nonconvex/multistart_partial_global | 全局命中率 <100% 且可复现（seed=42） | 99/200 = 49.5%，两侧盆地均有落点 | PASS |
| E3 | kkt/equality_qp_multiplier_recovery | 数值解 KKT 系统恢复 λ 并对账解析解 | λ=0.500000（解析 0.500000），res=0 | PASS |
| E3 | kkt/equality_qp_multiplier_recovery_n3 | n=3 同上 | λ=0.380952（解析 0.380952），res=0 | PASS |

## 总判定

- PASS: 7 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 结论与备注

- E1 口径说明：本轮修复将测量从"函数值比"（渐近值为理论平方）改为**误差范数比**，与理论值 (κ−1)/(κ+1) 同口径后三个 κ 的相对误差均为 0.0%，无需再放宽容差。旧 `sd_factor.csv`（est=0.814@κ=10，范数比口径）与现行实现恢复一致，本轮数据由现行代码重算。
- E3 修复说明：原测试把已知答案代回验证（同义反复）；现为真实数值过程——组装 KKT 线性系统、LU 求解、恢复 (x,λ) 并与解析解对账（复用 vendor A1 `mlab_lu_solve_dense`）。
- 局部方法不能保证非凸全局最优，必须依赖 C 层全局搜索（理论必然性实证）。
