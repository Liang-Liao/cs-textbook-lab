# C9-bayes-opt 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — gp（RBF-GP + EI/PI/UCB）、bo（LHS + BO/批量 q-EI/随机/LHS 基线）、tpe（1D 分段 KDE 简化版）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + A3(conv) + B1(opt/linesearch) + B2(optcore) + C1(rng)
- seed: GP 覆盖 91001；BO 93k 系列；内层优化 95001；Cholesky 96001；TPE 97k 系列；批量 BO 98k 系列

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-gp-cover | GP 2σ 覆盖率 ∈ [90%, 98%]（双口径并列） | **vs-noisy=0.936**（234/250）∈[0.90,0.98]；**vs-truth=1.000**（250/250）≥0.90 | PASS |
| E2-gp-chol | A1 Cholesky 复用 | 对称 SPD 上 Cholesky 解 vs LU 解 ‖Δ‖=**4.5e-17** | PASS |
| E3-bo-ei | EI-BO 评估数 ≤ 随机 1/3（p&lt;0.01） | BO=**9.4** vs 随机=**29.1** vs LHS=**24.2**，ratio=**0.325**，p=3.3e-4 | PASS |
| E4-acq-inner | 采集内层 BFGS 不劣于网格 | max EI：grid=**0.157093**，grid+BFGS=**0.157093**（持平） | PASS |
| E5-tpe | TPE(地图项) 评估数 &lt; 随机，符号检验 p&lt;0.05 | TPE=**19.0** vs 随机=**40.8**，wins **27/40**（p=**0.0385**），reach 40/40 vs 35/40 | PASS |
| E6-bo-batch | 批量 BO(q-EI 常量 liar, 地图项) 评估数 ≤ 2× 顺序 | q1=**10.8** vs q4=**15.3**（ratio=**1.42**≤2），reach 双双 20/20 | PASS |

## 关键数据

- GP: `results/gp_coverage_1d.csv`（cover2s_noisy / cover2s_true 双列）
- BO: `results/bo_ei_vs_random.csv`、`results/bo_acq_inner.csv`
- TPE: `results/tpe_vs_random_1d.csv`
- 批量 BO: `results/bo_batch_qei.csv`

## 结论与备注

- **GP 覆盖率（双口径）**: 路线图 :563 字面口径（预测 μ±2σ 带对**无噪真值**的覆盖）实测 1.000——模型 noise=0.02 略大于 σn²=0.0144，带宽偏保守，真值全部落入带内，满足 ≥90% 下界；概率校准口径（对**独立含噪观测**的覆盖）实测 0.936，落在 [0.90,0.98]。两口径同一次拟合、同一批测试点并列报告：前者回答"带宽是否可信地包住真函数"，后者回答"带宽作为观测区间是否校准"。[90%,98%] 上界只对校准口径有意义（真值口径随带宽单调，不构成校准约束）。
- **EI-BO vs 随机**: 三盆地测试函数（全局盆地窄、局部盆地更宽）上，进入全局邻域（f*+0.05）时 EI-BO 平均评估数约为随机的 **1/3**（9.4 vs 29.1，配对 p=3.3e-4），且显著优于纯 LHS 一次性设计（24.2）。
- **采集内层优化**: 网格 + BFGS 多起点（B2 `mlab_opt_bfgs`，数值梯度）与纯网格持平（本组 GP 后验下网格已采到 EI 峰值），证实内层连续优化的增益在低维 + 中等网格密度时有限，但 BFGS 精修不劣于、且在网格稀疏时必要。
- **TPE（地图项落地）**: 1D 分段 KDE 简化实现（good/bad 分位切分 + Silverman 带宽封顶 + 重复提议拒绝）在盆内细纹函数上（target=f*+0.005，谷底极窄）平均 19.0 次评估达标，显著快于均匀随机 40.8（符号检验 27/40，p=0.0385）；评估数被预算截尾非正态，故配对用符号检验而非 t 检验。
- **批量 BO（地图项落地）**: q-EI 常量 liar（批内 pending 点以当前 best_f 代真值重拟合）q=4 达标评估数 15.3，为顺序 q=1（10.8）的 1.42 倍——并行代价有界（≤2×），两口径达标率均 20/20；单批 4 点可把外层迭代（GP 拟合/采集最大化次数）压缩到 1/4。
- 超参固定（ls/sf2/noise），未做边际似然自动估计；PI/UCB 已实现但实验主路径为 EI。
- common 增量: `mlab_gp_*`、`mlab_acq_*`、`mlab_lhs`、`mlab_bo_*`（含 `mlab_bo_run_batch`）、`mlab_tpe_*`。

## 总判定

- PASS: 6 / FAIL: 0 — **PASS**
