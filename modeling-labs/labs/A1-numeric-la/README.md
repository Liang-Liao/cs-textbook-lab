# A1 数值计算与线性代数

路线图地基层：浮点与误差、矩阵运算、LU/Cholesky、数值微积分、迭代法。

## 知识点

- 浮点表示、机器精度、舍入误差；病态与条件数（条件数估计：幂迭代）
- 向量/矩阵运算、范数
- 部分主元 LU（奇异拒绝）；Cholesky（SPD 分解与求解）
- 迭代法：Jacobi、Gauss-Seidel、SOR（松弛因子、谱半径，路线图 L102）
- 数值微分：前向/中心/三点前向差分与最优步长（U 形曲线）
- 数值积分：复化 Simpson 与收敛阶

## 判据（路线图 L112–115 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 可控条件数矩阵 + LU 求解 | LU 求解残差 ‖Ax−b‖/‖b‖ < 1e-12 × 条件数（双精度） | κ=1e6 时 1.08e-15 < 1e-6 PASS |
| E2 | 差分误差-步长曲线 | 数值微分误差曲线呈 U 形，实测最优步长与理论 √ε 量级相符（比值落在 [0.3, 3]） | ratio=1.193 PASS |
| E3 | 复化 Simpson 收敛阶 | Simpson 实测收敛阶（log-log 回归斜率）= 4 ± 0.3 | 3.9973 PASS |
| E4 | Cholesky | SPD 重构误差小；非 SPD / 奇异被拒绝 | 4.3e-16 PASS |
| E5 | 迭代法 | 严格对角占优 / SPD 收敛；GS 迭代数≈Jacobi 一半（ρ_GS=ρ_J²）；最优 ω SOR 显著快于 GS；谱半径>1 发散被检出 | 见 report |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | vec / mat / linalg / numcal / numiter |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | 无（起点 lab） |
| `results/` | 全量运行自动重算落盘（fd_u_shape / simpson_convergence / lu_residual / cholesky / numiter_iters） |

测试 suite：linalg（多 κ / 奇异 / SPD / 条件数）、numcal（U 形 / 阶 / 对照）、numiter（Jacobi/GS/SOR）、matvec

```bat
mingw32-make check-A1
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/A1-numeric-la check TEST_ARGS=<suite|suite/case>
```
