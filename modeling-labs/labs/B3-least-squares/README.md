# B3 最小二乘与模型拟合

正规方程、QR、LM、置信区间覆盖率、加权/Huber 稳健回归、残差诊断。

## 知识点

- 线性 LS 两种解法对照：正规方程（κ² 条件放大）vs QR（MGS）
- 非线性 LM 参数恢复与初值收敛域扫描
- 近似 95% CI 与 500 次重复覆盖率；3σ 参数恢复（SE 口径）
- 加权 LS（异方差）、Huber 稳健回归（IRLS）、残差诊断 R²（路线图 L233）

## 判据（路线图 L241–244 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 3σ 参数恢复 | 参数恢复误差全部落在 3σ 内（SE 口径，见 report 注） | max\|z\|=3.54，P(\|z\|>3)=0.75% PASS |
| E2 | QR vs 正规方程 | 病态设计矩阵上 QR 误差显著小于正规方程（κ² 放大，L230） | 4.6e-10 vs 5.6e-9 PASS |
| E3 | LM 初值扫描 | LM 在给定初值邻域内（该邻域经扫描确定并记录）收敛成功率 = 100% | 4 参模型 ±20% 邻域 100% PASS |
| E4 | CI 覆盖率 | 95% CI 实测覆盖率落在 [88%, 99%]（500 次重复，名义 95%） | 95.0%/95.2% PASS |
| E5 | 稳健回归 | 异方差下 WLS 优于 OLS；离群点下 Huber 优于 OLS | 全部 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | lsq（正规方程/QR/LM/协方差/R²/加权/Huber） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1 + A2(dist/stats/rng) |
| `results/` | 全量运行自动重算落盘（qr_vs_ne / linear_recovery / ci_coverage / lm_sweep / lm_fit_sweep） |

测试 suite：lsq、lm、ci、robust

```bat
mingw32-make check-B3
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B3-least-squares check TEST_ARGS=<suite|suite/case>
```
