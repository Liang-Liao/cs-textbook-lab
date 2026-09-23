# B2 牛顿型方法、共轭梯度与信赖域

Newton/BFGS/L-BFGS/DFP/CG/Dogleg/Nelder-Mead/坐标下降/Hooke-Jeeves/DIRECT 与基准函数。

## 知识点

- 牛顿法 + Hessian 正定化（无 Hessian 时有限差分 fallback）
- DFP/BFGS/L-BFGS 拟牛顿族（双循环递推、曲率对失效时清空重建）
- 线性 CG 有限步终止；收敛阶测量（f_hist 上的阶估计器）
- 信赖域 / Dogleg（Cauchy 点、τ 求解、半径更新）
- 坐标下降、Nelder-Mead、Hooke-Jeeves、DIRECT（无导数/全局，路线图 L203-205）
- 加性噪声下无导数方法 vs 梯度法对照（L220）

## 判据（路线图 L215–220 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | CG | CG 在 n 维二次问题上 ≤ n 步内残差 < 1e-10 | n=12：12 步，res=2.9e-18 PASS |
| E2 | 收敛阶 | 实测收敛阶：牛顿法 2 ± 0.3；BFGS > 1.2（超线性证据） | 牛顿 2.022；BFGS 1.351 PASS |
| E3 | 评估数 | Rosenbrock 上 BFGS 函数评估数显著少于最速下降（差距 ≥ 5 倍） | 125 vs 4721（fevals 真实计数）PASS |
| E4 | 信赖域 | 信赖域方法在病态起点集合上的失败率低于纯线搜索，跨 ≥10 起点可复现 | 12 起点：GD 12 败 vs Dogleg 0 败（κ=100）PASS |
| E5 | NM 噪声 | 加性噪声下 NM 终值误差 ≤ 噪声幅 3 倍，梯度法发散/显著劣化 | amp=0.05：NM 0.146 vs GD 3.77 PASS |
| E6 | 无导数族 | 坐标下降/HJ/DIRECT 收敛性对照 | 全部 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | optcore（Newton/BFGS/L-BFGS/DFP/CG/Dogleg/NM/CD/HJ/DIRECT）、bench |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1 + A2 + A3(conv) + B1(linesearch/opt) |
| `results/` | 全量运行自动重算落盘（conv_order / noise_nm / tr_vs_gd） |

测试 suite：cg、newton、bfgs、lbfgs、dfp、trust_region、dogleg、nelder_mead、coord_descent、hooke_jeeves、direct、bench、conv

```bat
mingw32-make check-B2
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B2-newton-quasi-tr check TEST_ARGS=<suite|suite/case>
```
