# M2 全局优化基准测试台

统一基准库 + SA/GA/DE/PSO（+CMA-ES/HYBRID）算法接口 + 混合全局-局部框架 + 算法选择参考表。

## 知识点

- 统一基准：Sphere / Rosenbrock / Rastrigin / Griewank / Ackley（2–20 维）
- 统一指标：成功率、AES（成功时评估数）、收敛曲线（等预算检查点重跑法）
- 四算法最小口径：SA、实数 GA、DE、PSO；扩展 CMA-ES 与 HYBRID
- 混合框架：全局粗搜（PSO，75% 预算）+ Hooke-Jeeves 模式搜索精修（25% + 全局提前停剩余）；总评估与单层同口径
- 参数敏感性热图批量化（5 函数×5F×5CR，PGM）；两比例检验

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 全表 | **4×5×3** 每格 ≥100 次，报告表完整 |
| E1b | 收敛曲线 | 6 算法×5 函数×9 检查点 CSV，终局均值 ≤ 首检查点 |
| E2 | 混合 | 等预算下多峰成功率显著优于任一单层（**p<0.01**），逐函数口径如实呈现 |
| E3 | 选择表 | 个人实测《算法选择参考表》，尾注数据背书 |
| E4 | 热图 | DE F×CR 参数敏感性批量化（5 函数，PGM） |

路线图原文锚点：

- 全组合（4 算法 × 5 函数 × 3 维度）成功率/AES 报告表完整且每格 ≥ 100 次运行
- 混合框架在多峰函数上的成功率显著优于任一单层算法（p < 0.01）
- 产出个人实测版《算法选择参考表》

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | gbench（函数库 + 算法适配 + GA + 混合） |
| `test/` | harness + test_main |
| `vendor/` | A1–A3, B1–B2, C1, C3, C5–C7 |

测试 suite：grid、curve、hybrid、report、heatmap

```bat
mingw32-make check-M2
bin\test.exe --list
bin\test.exe grid
bin\test.exe curve
bin\test.exe hybrid/beats_each_single_on_multimodal
bin\test.exe report
mingw32-make -C labs/M2-global-bench check TEST_ARGS=<suite|suite/case>
```
