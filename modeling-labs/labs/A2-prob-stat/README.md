# A2 概率与统计

C 层随机算法理论源头；KS/卡方等判据工具、估计量与置信区间、方差缩减。

## 知识点

- 常见分布与矩；CDF/PDF/分位数；erf/erfc
- CLT 与蒙特卡洛直觉（直方图 + 卡方正态拟合）
- 估计量与置信区间、最大似然估计（MLE，B3 伏笔）
- KS 检验、卡方拟合优度
- 方差缩减四种：控制变量、对偶变量、公共随机数、分层采样（路线图 L130）

## 判据（路线图 L137–139 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | CLT：指数样本均值 | 直方图与正态对比卡方检验不拒绝 + 矩贴近理论 | p=0.0226>0.01 PASS |
| E2 | KS 一类错误率 | 自实现 KS 检验在 1000 次重复实验中对符合原假设的样本拒绝率落在 [0.03, 0.07]（α=0.05 标称） | unif=0.053 PASS |
| E3 | 控制变量法 | 控制变量法方差缩减比与理论公式值相对误差 < 10%；最优 c* 优于 plain/2c* | 0.0168 vs 0.0163（3%）PASS |
| E4 | 其余三种方差缩减 | 对偶/公共随机数/分层均显著降方差（VR 实测与理论一致量级） | 0.031/0.003/0.011 PASS |
| E5 | MLE 与 CI | 正态 MLE 矩恢复；z-CI 500 次覆盖率≈0.95；宽度 ∝1/√n | 0.956 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | dist / stats（含 MLE/CI）/ gof / rng |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | 无 |
| `results/` | 全量运行自动重算落盘（ks_reject / clt_hist / clt_chi2 / variance_reduction / mle_ci） |

测试 suite：dist、stats、gof、rng、clt、control_variate、variance_reduction、estimation

```bat
mingw32-make check-A2
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/A2-prob-stat check TEST_ARGS=<suite|suite/case>
```
