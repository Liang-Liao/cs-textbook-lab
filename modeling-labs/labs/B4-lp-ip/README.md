# B4 线性与整数规划

两阶段修订单纯形（Bland 规则）、对偶与互补松弛、顶点枚举互验、0-1 背包与指派分支定界、Gomory 割平面演示。

## 知识点

- LP 标准形、基可行解=顶点；两阶段单纯形 + Bland 防循环（Phase II 禁人工列进基）
- 对偶理论：y = c_B·B⁻¹、原始-对偶间隙、互补松弛（路线图 L254-256）
- 分支定界：0-1 背包（分数上界）与指派问题（行最小下界）
- 割平面概览：纯整数 Gomory 割的可运行最小演示（L257）

## 判据（路线图 L264–266 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | 原始-对偶 | 原始-对偶目标间隙 = 0（双精度直接比较） | gap=0，comp=0 PASS |
| E1b | 互验 | 单纯形 vs 顶点枚举逐实例一致（L260） | 30/30 一致 PASS |
| E2 | 对账 | 分支定界与暴力枚举 100/100 实例解一致 | 100/100 PASS |
| E2b | 指派 | 指派 BnB 与全排列枚举 50 实例一致（L262） | 50/50 PASS |
| E3 | 割平面 | Gomory 割闭环到整数最优并与暴力枚举对账（L257） | 3 割收敛 (3,1)/11 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | simplex（两阶段）、knapsack（BnB/暴力/顶点枚举）、assign（指派 BnB/枚举）、cuts（Gomory 演示） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1 + A2(rng 等) |
| `results/` | 全量运行自动重算落盘（lp_compare / knapsack_100 / knapsack_summary / assign_50 / lp_enum_mismatch） |

测试 suite：simplex、knapsack、assign、cuts

```bat
mingw32-make check-B4
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B4-lp-ip check TEST_ARGS=<suite|suite/case>
```
