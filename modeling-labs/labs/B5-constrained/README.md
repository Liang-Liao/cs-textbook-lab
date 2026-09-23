# B5 约束非线性优化

外点罚、增广拉格朗日、对数障碍内点法、投影梯度、箱式 QP、最小 SQP。

## 知识点

- 外点罚函数（病态化）：真 KKT 残差（含罚导出乘子 λ=2μ·max(0,g)）
- 增广拉格朗日：乘子更新、达同精度所需 σ ≪ 纯罚 μ
- 内点法/障碍函数（λ=μ/(−g)，中心路径）与最小 SQP（有效集 QP 子问题，路线图 L277/279）
- 投影梯度（投影 Armijo 回溯 + 发散保护）与有效集枚举互验

## 判据（路线图 L286–288 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 罚系数扫描 | 增广拉格朗日达到与纯罚相同解精度所需罚系数 ≤ 纯罚方案的 1% | σ_min=1 vs μ_min=1e4（0.01%）PASS |
| E2 | KKT 残差 | 最终解 KKT 残差 < 1e-6 | 6.05e-09 PASS |
| E3 | 投影梯度 | 投影梯度与有效集枚举解一致（100 个随机箱式问题） | 100/100 PASS |
| E4 | 内点/SQP | 对数障碍与最小 SQP 收敛到 KKT 点 | kkt≈e-8 级 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | nlp（罚/AL/投影梯度/障碍/SQP）、box_qp |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1 + A2 + A3 + B1 + B2 |
| `results/` | 全量运行自动重算落盘（pen_vs_al / box_rand100 / barrier_sqp） |

测试 suite：penalty、aug_lag、proj_grad、box_qp、interior、sqp

```bat
mingw32-make check-B5
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B5-constrained check TEST_ARGS=<suite|suite/case>
```
