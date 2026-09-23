# C3 模拟退火

Metropolis 准则 + 几何降温的连续/组合 SA；Rastrigin 调参与 TSP 邻域对比。

## 知识点

- Metropolis 接受率 min(1, exp(-ΔE/T))；温度的角色
- 几何降温 α；初始温度标定（目标接受率反推）
- 连续邻域：高斯扰动幅度 ∝ T；组合邻域：swap / 2-opt（2-opt 恒等/等长移动跳过）
- 接受率-温度诊断；重加热与多重启（已实现）
- 经典对数降温 T=T0·ln2/ln(1+k)（已实现；实测冷不到冻结温度）

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 2D Rastrigin | 调参后 1000 次命中率 ≥ 80% |
| E2 | 参数扫描 | α×inner×T0 热图完整；同参复跑 \|Δ\|<5pp |
| E3 | TSP 邻域 | 2-opt 显著优于 swap，配对 p<0.01 |
| E4 | 接受率诊断 | 接受率随 T 下降；有效降温窗口非空 |
| E5 | T0 标定 | 目标 0.8 → 首层接受率 0.8±0.25；目标调低后接受率显著下降 |
| E6 | 重加热/多重启 | 短预算下多重启 best-of-8 命中率 > 单跑 +15pp |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | sa / tsp / bench_sa(Rastrigin) |
| `test/` | harness + test_main |
| `vendor/` | A2 + C1(rng/mc) + B1(opt) |

测试 suite：sa、tsp

```bat
mingw32-make check-C3
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/C3-sa check TEST_ARGS=<suite|suite/case>
```
