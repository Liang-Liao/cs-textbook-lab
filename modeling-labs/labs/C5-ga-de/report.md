# C5-ga-de 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — ga（二进制 GA，精英真 top-k 去重）、de（rand/1、best/1、cur2best/1；父代 fit 缓存）
- vendor: A2(dist/stats/gof) + C1(rng) + C3(bench_sa) + B2(bench)
- seed: GA 55k–58k；DE 64k–66k / 70k 网格

## 本轮修复对口径的影响

- **DE 评估数**：修复前每代对父代整群重评估（n_eval = pop×(2·max_gen+1)），修复后父代
  fit 只在初始代评估、由选择步维护，n_eval = pop×(max_gen+1)（与 de.h 声明一致）。
  搜索轨迹不受影响——`de_f_cr_grid.csv` 逐格数值与修复前完全相同。
- **DE best/1 语义**：修复前 best_i 仅在"出现新全局最优"的代更新、其余代退化为
  pop[0]，即旧 best/1 实际多为 pop[0]/1；修复后每代取种群 fit 的 argmin，best/1
  首次成为真正的 best/1，E3/E3b/E5 结果按新语义重测。
- **GA 精英**：修复前"精英"重复拷贝同一最优个体；修复后真 top-k 去重。

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-ga-onemax | GA 解 OneMax；deceptive 易早熟 | OneMax 30/30 全 1；deceptive 0/30（陷在次优，fit=37.97/40） | PASS |
| E2-ga-diversity | 早熟多样性**跌落时刻**中位数显著更早（:480） | 跌落时刻 med：早熟 5 代 vs 成功 14 代，p=1.0e-12（div@gen3 med 0.622 vs 0.899） | PASS |
| E3-de-reversed | 单峰/多峰优势相反，p<0.01（:479） | Sphere10D F=0.7：best/1 5.5e-11 ≪ rand/1 2.9e-3，p=5.6e-7；Rastrigin10D F=0.3：rand/1 6.18 ≪ best/1 37.4，p<1e-12 | PASS |
| E3b-rosenbrock | Rosenbrock（路线图实验 2 点名）变体对比 | rand/1 5.06 ≪ best/1 64.0，p=2.9e-10——形式单峰的弯曲谷惩罚贪婪 best/1 | PASS |
| E4-de-grid | F×CR 全表，每格≥50 次 | 5×6=30 格 ×50 次，成功/失败格皆有；`de_f_cr_grid.pgm` | PASS |
| E5-de-div | best/1 多样性塌缩更早 | 中位 first_low_gen best1=8 vs rand1=201，p=2.9e-11 | PASS |

## 关键数据

- GA: `results/ga_onemax_deceptive.csv`、`results/ga_diversity_drop.csv`（含 first_low_gen 列）
- DE 网格: `results/de_f_cr_grid.csv` + `results/de_f_cr_grid.pgm`
- DE 变体: `results/de_rand_vs_best.csv`（Sphere+Rastrigin 逐 seed 配对）、
  `results/de_rosenbrock_rand_vs_best.csv`（新增）、`results/de_diversity_drop.csv`
- 成功判据（网格）: 10D Rastrigin 上 best f < 1e-2；DE/rand/1，pop=40，gen=200

## 结论与备注

- **GA**: OneMax 无欺骗结构，锦标赛+均匀交叉可稳定全收敛；陷阱函数在轮盘+低变异下锁死次优（全 0 盆地），多样性跌落时刻中位数 5 代 vs 成功组 14 代，差异极显著。
- **DE 变体（按修正后 best/1 语义）**: 优势反转在"问题类 × folklore 参数"语境下成立——单峰 Sphere 配大 F=0.7 时 best/1 终态好 4 个数量级；多峰 Rastrigin 配小 F=0.3 时 rand/1 显著更优（best/1 收缩种群致差分萎缩、停滞在更差局部最优）。Rosenbrock 虽形式单峰，但弯曲谷惩罚贪婪追踪，rand/1 反而显著更优——「单峰→best/1」二分法有边界。固定 F=0.5 时本实现两个问题均 best/1 占优（E4 网格同口径），反转结论依赖参数类语境，如实记录。
- **参数表（以 CSV 为准）**: 10D Rastrigin 上 **F=0.2×CR=0.0 最优（成功率 0.88）**，F=0.4×CR=0 次之（0.76）。低 CR 每次试验只改少数维 → 近似逐维搜索，而 Rastrigin 完全可分离，低 CR 占优合理；高 CR（≥0.8）在所有 F 下成功率≈0。 folklore「多峰用中高 F×高 CR」在本实现/该预算下不成立。
- F×CR 交互、自适应 DE、EDA/CE/Memetic 为注记项，未实现。
- common 增量: `mlab_ga_*`、`mlab_de_*`。

## 总判定

- PASS: 6 / FAIL: 0 — **PASS**
