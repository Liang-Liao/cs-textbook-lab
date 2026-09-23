# M2-global-bench 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — gbench（统一基准 + 算法适配 + 实数 GA + 混合框架）
- vendor: A1+A2+A3+B1(opt/linesearch)+B2(optcore/bench)+C1(rng)+C3(sa/bench_sa)+C5(de)+C6(cmaes/es)+C7(pso)，R6/R7 已 sync
- seed: 全表 970k 系列；曲线 996k；混合 980k；热图 995k；选择表扩展 990k–993k
- 主表预算: budget=900 评估/次；混合对比 budget=1000（**等预算对齐**）；每格/每组 **100 次**独立运行

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-grid | **4 算法 × 5 函数 × 3 维**全表，每格 ≥100 次 | 60 格 × 100 次；DE/GA/SA mean_evals=900（DE 修复前 1780 超支），PSO≤900（命中即停），`m2_full_table.csv` 完整 | PASS |
| E1b-curve | 收敛曲线 CSV（R7 补） | 6 算法 × 5 函数 × 9 检查点 × 60 次=270 行，各 (algo,func) 终局均值 ≤ 首检查点（整体趋势下降），`m2_convergence_curves.csv` 含真实评估数列 | PASS |
| E2-hybrid | 混合框架多峰成功率显著优于**任一**单层（p&lt;0.01），等预算对齐 | 等预算 1000：HYBRID **269/300** vs SA 4 / GA 77 / DE 0 / PSO 0，max p≈0；逐函数：Rastrigin **99/100**、Ackley **100/100**、Griewank **70/100**（GA 77 反超，如实呈现） | PASS |
| E3-select | 个人实测《算法选择参考表》，尾注有本轮数据 | `results/m2_algo_selection.md`：15 格主表 + Rosenbrock/Rastrigin 扩展抽样（HYBRID/CMA-ES）；尾注按实测措辞 | PASS |
| E4-heat | 参数敏感性热图（R7 批量化） | DE F×CR × 5 函数 × 30 次 = 125 行 + 5 张 PGM；成功率分母改 n_run（去条件偏差） | PASS |

## 主表成功函数算法（节选，完整见 CSV 与选择参考表）

| 函数 | n | 推荐（成功率最高） |
|---|---|---|
| sphere | 2 / 5 | **PSO（100%）** |
| sphere | 10 | PSO 45% |
| rosenbrock | 2 | DE 93% |
| rosenbrock | 5 / 10 | 单层全部 0% → 需更大预算或精修框架 |
| rastrigin | 2 | PSO 83% |
| rastrigin | 5 / 10 | 单层全部 0% → **HYBRID（扩展抽样 97%）** |
| griewank | 2 | PSO 100% |
| griewank | 5 / 10 | **GA 98%** |
| ackley | 2 / 5 | **PSO 100%** |
| ackley | 10 | PSO 80% |

## 关键数据

- 全表: `results/m2_full_table.csv`（60 行；mean_evals 列可核预算）
- 收敛曲线: `results/m2_convergence_curves.csv`（R7 新增；6 算法 × 9 检查点，mean/median best_f + 真实评估数）
- 混合对比: `results/m2_hybrid_multimodal.csv`（含 mean_evals 列，等预算可核）
- 选择参考表: `results/m2_algo_selection.md`
- DE 热图: `results/m2_de_fcr_scan.csv` + `results/m2_de_fcr_<func>.pgm` ×5（R7 批量化，替代旧 rastrigin 单图）

## 结论与备注

- **统一接口**：`mlab_galgo_run` 适配 SA（C3）、实数 GA（M2 自生长）、DE（C5）、PSO（C7）、CMA-ES（C6）与 HYBRID。C 层算法成功返回 1，M2 统一归一为 0=成功。
- **DE 预算标定重验（R7）**：C5 修后 DE 评估口径为 `pop×(gens+1)`（父代 fit 缓存、每代只评 trial），主表 mean_evals 恰为 900。修复前 vendored 旧正本整群重评估实际耗 1780 评估（≈2×名义预算），本轮 60 格主表全部按修后口径重跑——DE 在紧目标（sphere n≥5、Rosenbrock n≥5）下成功率显著回落属真实等预算表现，而非数据漂移。
- **等预算对齐（R7）**：旧 HYBRID 全局用满 budget 再叠加精修（实际 ≈2.5–5× 名义预算，n_eval 含 `local_budget/3` 折算项）。现改为总预算 = budget：全局 75%（PSO 命中 target 提前停时剩余自动流转精修）+ Hooke-Jeeves 模式搜索精修（step0=0.1×盒宽、失败减半、只采纳改进），n_eval 全程真实计数（含 HJ 初始求值与盒裁剪核验各 1 次）。旧 NM 精修在等预算下迭代数不足（ackley 0/100），HJ 变体在探针对比（3 函数 × 40 次 × 6 变体）中全面占优后采纳。
- **逐函数如实呈现（R7）**：Griewank n=5 上 GA 77/100 反超 HYBRID 70/100——GA 的种群多样性在该函数上仍优于「PSO 粗搜+精修」；HYBRID 的优势集中在 Rastrigin（99 vs 单层 0）与 Ackley（100 vs 单层 0），聚合 p≈0 由前两者驱动，不掩盖 Griewank 的反例。
- **算法选择参考表**：数据驱动——低维单峰选 PSO/DE；Griewank 类 GA 稳健；紧目标高维多峰选 HYBRID。尾注改为实测背书：扩展抽样显示 Rosenbrock n=5 在 900 预算下 HYBRID/CMA-ES 均 0%（旧尾注「Rosenbrock 优先 HYBRID/CMA-ES」无数据支持，已删除）；Rastrigin n=5 HYBRID 97% vs CMA-ES 0%。
- **热图批量化 + 去条件偏差（R7）**：扫描从单函数扩为 5 函数（125 行合并 CSV + 每函数 PGM）；成功率分母由「de_run 返回 1 的 n_ok」改为 n_run，消除误差运行被静默剔除的条件偏差。
- **收敛曲线口径（R7）**：各检查点用同 seed 独立完整重跑（预算=c 的可达 best_f），不依赖「截断=前缀」假设——PSO 的 w 线性调度依赖 max_gen，混合的阶段分配随预算变化，均非前缀；PASS 条件为终局均值 ≤ 首检查点（整体趋势下降）。小预算下 SA/HYBRID 因内层批量/阶段守卫实际评估略高于名义检查点，真实值在 mean_evals_actual 列如实呈现。
- **GA 预算钳制（R7）**：子代数钳制到剩余预算（nchild 由死变量变为实际生效），budget=1000 时 GA 恰好 1000 评估（修复前 1050 超支 5%）；精英替换仅遍历已生成子代，避免读取未初始化 fitc。
- 注记：主表 GA 为 M2 实数编码（锦标赛+算术交叉+高斯变异）；C5 二进制 GA 面向离散问题，未纳入连续主表。路线图「GA」口径以本实数 GA 落地。
- common 增量: `mlab_gfunc_*`、`mlab_galgo_run`、`mlab_real_ga_run`、`mlab_hybrid_run`（等预算）、`mlab_two_prop_p_one_sided`。

## 总判定

- PASS: 5 / FAIL: 0 — **PASS**
