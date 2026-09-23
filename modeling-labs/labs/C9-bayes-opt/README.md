# C9 贝叶斯优化与代理模型

高斯过程回归（RBF + A1 Cholesky）、EI/PI/UCB 采集、BO 循环、批量 BO（q-EI 常量 liar）与 TPE 简化实现；与均匀随机 / LHS 对账。

## 知识点

- GP：核函数、后验均值/方差、噪声项；`K + σ_n² I` 的 Cholesky
- 采集函数：EI（最小化闭式）、PI、UCB；探索-利用权衡
- BO：初始设计（LHS）→ 拟合 GP → 最大化采集 → 真评估
- 批量 BO：q-EI 常量 liar（pending 点以当前最优代真值重拟合再提出）
- 代理家族地图：TPE 简化实现（1D 分段 KDE，good/bad 分位切分）
- 内层连续优化：网格 vs BFGS 多起点（B2 optcore 复用）

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | GP 覆盖率（双口径） | **2σ 覆盖率 ∈ [90%, 98%]**（校准口径）；无噪真值口径 ≥90% 并列报告 |
| E2 | Cholesky | A1 分解/回代与 LU 解一致 |
| E3 | EI-BO vs 随机 | 达到最优 **90% 水平**所需评估数 **≤ 随机 1/3**（20 次，p&lt;0.01） |
| E4 | 内层优化 | grid+BFGS 最大化 EI **不劣于**纯网格 |
| E5 | TPE（地图项） | 达标评估数 **&lt; 随机**（40 次配对符号检验 p&lt;0.05） |
| E6 | 批量 BO（地图项） | q-EI 常量 liar q=4 达标评估数 **≤ 2× 顺序** |

路线图原文锚点：

- GP 2σ 覆盖率落在 [90%, 98%]
- EI-BO 达到目标解质量所需评估数 ≤ 随机搜索的 1/3（20 次重复，配对检验 p < 0.01）
- 代理家族概览：随机森林（SMAC）、TPE（Hyperopt）——地图级（本 lab 以 1D 分段 KDE 最小实现落地）
- 批量 BO 与成本感知概览（地图）（本 lab 以 q-EI 常量 liar 最小实现落地）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | gp（RBF-GP + 采集）、bo（LHS + BO/批量 q-EI/随机/LHS 基线）、tpe（1D KDE） |
| `test/` | harness + test_main |
| `vendor/` | A1(linalg) + A2 + A3(conv) + B1(opt/linesearch) + B2(optcore) + C1(rng) |

测试 suite：gp、bo、tpe、batch

```bat
mingw32-make check-C9
bin\test.exe --list
bin\test.exe
bin\test.exe gp
bin\test.exe bo/ei_bo_evals_le_one_third_random
bin\test.exe tpe batch
mingw32-make -C labs/C9-bayes-opt check TEST_ARGS=<suite|suite/case>
```
