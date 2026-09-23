# A3 凸分析与优化理论

收敛因子、非凸多起点、KKT 乘子数值恢复。

## 知识点

- 强凸二次最速下降收敛因子 vs (κ−1)/(κ+1)（误差范数口径，与理论同口径对比）
- 非凸局部法命中率 <100%（无免费午餐实证）
- 等式约束 QP 的 KKT 系统：数值求解并恢复乘子 λ（路线图 L159）
- 收敛率语言（线性/次线性/超线性/二次）估计器

## 判据（路线图 L161–163 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 最速下降收敛因子 | 最速下降实测收敛因子与理论值相对误差 < 20%（至少三个不同 κ） | κ=10/50/200 相对误差 0.0% PASS |
| E2 | 双谷多起点 | 局部方法命中全局最优的比例 < 100% 且随起点分布可复现（非凸性实证） | 49.5%，seed=42 PASS |
| E3 | KKT 乘子恢复 | QP 的 KKT 乘子恢复：数值解 KKT 线性系统得 λ 并与解析解对账 | λ=0.5 / 0.380952 精确一致 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | conv（收敛因子/阶估计） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1（linalg/vec/mat）+ A2（rng 等） |
| `results/` | 全量运行自动重算落盘（sd_factor / multistart / kkt_recovery） |

测试 suite：conv、nonconvex、kkt

```bat
mingw32-make check-A3
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/A3-convex-opt check TEST_ARGS=<suite|suite/case>
```
