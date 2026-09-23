# M1 曲线拟合工作台

双引擎（LM + BFGS）拟合平台：合成数据 → 拟合 → 协方差/CI → 覆盖率检验 → 自动报告。

## 知识点

- 非线性最小二乘：LM（B3）vs BFGS 最小化 0.5·RSS（B2）
- 参数协方差 ≈ σ²(JᵀJ)⁻¹；95% CI 与覆盖率统计口径
- 噪声水平 σ 与恢复误差的线性关系（理论 se ∝ σ）
- 自动报告：参数表、协方差矩阵、残差统计落盘

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 流水线 | 双引擎拟合 + 报告落盘 |
| E2 | 覆盖率 | 500 次重复 95% CI ∈ **[88%, 99%]** |
| E3 | 收敛域 | LM/BFGS 成功率 **100%**（初值 ±20%） |
| E4 | σ 扫描 | 误差随 σ 线性放大，log-log 斜率 ≈ 1 |
| E5 | 理论对照 | mean\|err\|/se ≈ 0.80 |

路线图原文锚点：

- 500 次重复实验 95% CI 覆盖率 ∈ [88%, 99%]
- LM 与 BFGS 在各自收敛域内成功率 100%
- 对 5 类不同噪声水平（σ 扫描）的参数恢复误差随 σ 线性放大（斜率 ≈ 理论值）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | fitbench（工作台） |
| `test/` | harness + test_main |
| `vendor/` | A1+A2+A3+B1+B2+B3+C1 |

测试 suite：pipeline、coverage、domain、noise

```bat
mingw32-make check-M1
bin\test.exe --list
bin\test.exe
bin\test.exe coverage/ci95_coverage_500_repeats
bin\test.exe noise
mingw32-make -C labs/M1-curve-fitting check TEST_ARGS=<suite|suite/case>
```
