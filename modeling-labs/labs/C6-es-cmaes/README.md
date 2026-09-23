# C6 进化策略与 CMA-ES

(1+1)-ES + Rechenberg 1/5 成功规则；简化 CMA-ES（进化路径、秩-μ、σ 自适应、协方差特征分解）；与 DE 同预算对账；旋转椭球上的协方差主轴对齐。

## 知识点

- 进化策略记法：(μ/ρ, λ)-ES 与 (μ/ρ + λ)-ES（comma vs plus；实现为多父代中间重组 + log-normal σ 自适应）
- 1/5 成功规则：步长按滑动窗口成功率相对目标 0.2 做乘性调整
- CMA-ES 核心：`m ← Σ wᵢ xᵢ`、进化路径 `p_σ / p_c`、秩-1 + 秩-μ 更新 `C`、`σ` 自适应
- 采样 `x = m + σ B D z`；`C = B D² Bᵀ`（本 lab 用循环 Jacobi 对称特征分解）
- IPOP 重启：总预算分段 + λ 倍增重启（达目标提前收敛剩余预算）
- 机制对照：DE 是个体差分；CMA 是分布学习（归纳偏置不同）
- RESTARS、jDE/SaDE、PSO 对照为注记项（PSO 在 C7）

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 1/5 规则 | (1+1)-ES 在 Sphere 晚段成功率落在 **0.2 ± 0.05** |
| E2 | σ 自适应 | 自适应比固定步长更贴近 1/5；末态步长已收缩 |
| E3 | CMA vs DE | Rosenbrock 上 CMA **评估数显著少于 DE**（配对 p < 0.01） |
| E4 | 机制归因 | Rastrigin 上 CMA/DE 评估数比 ≥ 2× Rosenbrock 基线（缩窄）或 DE 终态显著更优（反转） |
| E5 | 协方差对齐 | 主轴与等高线长轴夹角随迭代收敛（Spearman(gen, angle) < 0，PGM 可复现） |
| E6 | (μ/ρ,λ) vs (μ/ρ+λ) | 两模式 Sphere 收敛（<1e-4）；plus 种群最优单调不回退 |
| E7 | IPOP 重启 | Rastrigin 同总预算下 IPOP 终态显著优于单次运行（p<0.01） |

路线图原文锚点：

- 1/5 成功规则使成功率达到目标区间 0.2 ± 0.05（自适应收敛实证）
- CMA-ES 在 Rosenbrock（强弯曲谷）上的函数评估数显著优于 DE（配对检验 p < 0.01），在 Rastrigin 上优势缩窄或反转（机制归因落到数据）
- 协方差主轴与函数等高线主轴夹角随迭代单调收敛（对齐过程可复现）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | es（(1+1)-ES/1/5）、cmaes（CMA-ES + symeig） |
| `test/` | harness + test_main |
| `vendor/` | A2(dist/stats/gof) + C1(rng/mc) + B2(bench) + C3(bench_sa) + C5(de) |

测试 suite：es、cmaes

```bat
mingw32-make check-C6
bin\test.exe --list
bin\test.exe
bin\test.exe es
bin\test.exe cmaes/cma_fewer_evals_than_de_rosenbrock
mingw32-make -C labs/C6-es-cmaes check TEST_ARGS=<suite|suite/case>
```
