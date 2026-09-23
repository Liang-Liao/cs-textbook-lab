# B7 不可微与近端方法（选读）

软阈值、次梯度法、近端点法、ISTA/FISTA、ADMM-LASSO、基追踪、带约束 QP 的 ADMM。

## 知识点

- 次梯度/次微分与次梯度法（α_k = α0/√k，路线图 L327）
- 近端算子与近端点法（L328）
- 软阈值/ISTA/FISTA（Beck-Teboulle 动量，L329-330）
- 基追踪 BP：min ‖w‖₁ s.t. Xw=y（ADMM + 仿射集投影，L331）
- ADMM 分裂求解 LASSO 与带等式约束 QP（与 B5 增广拉格朗日同题对账，L336/341）

## 判据（路线图 L338–341 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 支持恢复 | LASSO 合成实例：ISTA 零分量恢复支持准确率 ≥ 90% | 100% PASS |
| E2 | FISTA 加速 | FISTA 达到 ISTA 同精度所需迭代数 ≤ ISTA 的 1/2 | 162 vs 1863 PASS |
| E3 | 对账 | ADMM 与增广拉格朗日终解相对差 < 1e-8 | 8.9e-16 PASS |
| E4 | 次梯度 | 次梯度法慢于 ISTA（同预算）但大量迭代后追近 | 50 步对比 PASS |
| E5 | BP | 无噪基追踪精确恢复稀疏解 | resid=4e-28，max\|dw\|=1.4e-15 PASS |
| E6 | 近端点 | 近端点法收敛到同一目标 | rel=0 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | prox（软阈值/ISTA/FISTA/ADMM/次梯度/近端点/BP/ADMM-QP） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1 + A2 + A3 + B1 + B2 + B5（AL 对账链） |
| `results/` | 全量运行自动重算落盘（subgrad_vs_ista / admm_al_reconcile） |

测试 suite：prox_op、ista、fista、subgradient、prox_point、basis_pursuit、admm

```bat
mingw32-make check-B7
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B7-prox-admm check TEST_ARGS=<suite|suite/case>
```
