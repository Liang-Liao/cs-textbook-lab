# C6-es-cmaes 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — es（(1+1)-ES/1/5、(μ/ρ,λ) comma/plus）、cmaes（路径 + 秩-μ + σ + Jacobi 特征分解 + IPOP 重启）
- vendor: A2(dist/stats/gof) + C1(rng/mc) + B2(bench) + C3(bench_sa) + C5(de)
- seed: ES 91k–92k / 98k；CMA/DE Rosenbrock 93k；Rastrigin 95k / 99k；对齐 97k

## 本轮口径更新

- C5 `de.c` 修复后，`run_de_evals_to_target` 的「评估数 ≈ pop×(max_gen+1)」公式由近似
  变为精确（旧实现每代整群重评估，实际耗 pop×(2·max_gen+1)）。DE 搜索轨迹不变，
  E3/E4 数值与上轮一致；自本轮起评估数为真实口径。

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-es-fifth | 1/5 规则成功率 → 0.2±0.05 | Sphere10D，n=25，晚段成功率 **0.194** | PASS |
| E2-es-sigma | 自适应步长收敛行为 | 自适应 late=0.190（err 0.010）vs 固定步长 0.005（err 0.195）；σ 终态收缩 | PASS |
| E3-cma-rosenbrock | CMA 评估数显著优于 DE（p<0.01） | 5D Rosenbrock 预算 2e4，目标 1e-6：评估数 CMA=**3676** DE=**18587**（真实口径），p≈0；成功 36/40 vs 4/40 | PASS |
| E4-cma-rastrigin | Rastrigin 上优势缩窄或反转（正向量化） | 10D，目标 8.0：评估数比 CMA/DE=**0.78** ≥ 2×Rosenbrock 基线 0.40；成功 7/0，终态 CMA=13.56 vs DE=39.14 | PASS |
| E5-cma-axis | 协方差主轴夹角收敛 | 旋转椭球 θ=30°：early(g0-2)=**21.0°** → learn(g8-28)=**2.5°**，Spearman=**-0.883** | PASS |
| E6-murl-comma-plus | (μ/ρ,λ) 与 (μ/ρ+λ) 实现 + 机制对照 | Sphere10D μ=5/ρ=2/λ=10，n=25：comma 5.0e-16 / plus 8.4e-16 均收敛；plus 种群最优单调不回退 25/25（构造保证），comma 允许回退 | PASS |
| E7-cma-ipop | IPOP 倍增重启与预算分配（:494） | Rastrigin10D 预算 4e4 均分 4 段、λ×(1,2,4,8)：单次 best=14.1 vs IPOP **1.93**（p≈0），成功 0 vs 3 | PASS |

## 关键数据

- ES: `results/es_fifth_rule_sphere.csv`、`results/es_sigma_adapt.csv`、`results/es_murl_comma_plus.csv`（新增）
- CMA vs DE: `results/cmaes_de_rosenbrock.csv`、`results/cmaes_de_rastrigin.csv`
- IPOP: `results/cmaes_ipop_rastrigin.csv`（新增）
- 主轴对齐: `results/cmaes_axis_align.csv` + `results/cmaes_axis_align.pgm`
- DE 评估数口径: 同 seed 前缀下对 `max_gen` 二分（失败记满预算）；计数为真实评估数

## 结论与备注

- **1/5 规则**: 滑动窗口成功率相对目标 0.2 做乘性步长调整后，(1+1)-ES 在球面晚段成功率稳定在 0.19 附近；固定步长时成功率迅速跌到近 0（σ 来不及跟上收敛）。
- **CMA vs DE**: Rosenbrock 弯曲谷上分布学习优势巨大（评估数约为 DE 的 1/5，成功率 36 vs 4）。Rastrigin 多峰上两者都更吃力，评估数之比抬升到 0.78（正向量化判据：≥2×基线 0.40），**评估数优势显著缩窄**；本实现中 CMA 终态 best 仍常优于探索参数 DE（F=0.9），属于「质量优势保留、评估数优势消失」，与机制归因一致。
- **comma vs plus**: plus 选择天然保证种群最优单调不回退；comma 只在子代选择、允许回退（σ 谱系多样性保留）。球面上两者终态同数量级，机制差异体现于轨迹而非终态。
- **IPOP**: 简化 CMA-ES 无内部停滞准则，重启采用「总预算均分 4 段 + λ 倍增」的显式预算分配；多峰 Rastrigin 上重启收益显著（14.1 → 1.93）。成功次数仍少（3/20），与本实现的简化程度相符。
- **协方差对齐**: 旋转椭球上 C 最大特征向量在十余代内贴合等高线长轴（易方向），学习窗口内夹角从 ~20° 收敛到 ~2.5°；σ 数值塌缩后的代不计入判据（主轴会因浮点噪声漂移）。
- CMA-ES 为简化可运行核心：秩-1 + 秩-μ、`p_σ/p_c`、σ 自适应、盒约束反射；未实现 RESTARS、步长/协方差惩罚等完整特性。
- PSO 对照、jDE/SaDE 为注记项（PSO 在 C7）。
- common 增量: `mlab_es1p1_*`、`mlab_es_murl_*`、`mlab_cmaes_*`、`mlab_symeig_jacobi`。

## 总判定

- PASS: 7 / FAIL: 0 — **PASS**
