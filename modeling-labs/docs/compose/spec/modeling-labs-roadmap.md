---
feature: modeling-labs-roadmap
status: done
updated: 2026-09-19
branch: master
commits: 7800c5a..M3
---

# 数学建模与参数优化 Lab 总体设计与规划

## Report

全路线 A→B→C→D→M1–M3 已实现完毕（25 个 lab，`mingw32-make check-<id>` 全 PASS）。收口 lab M3：Lotka-Volterra 端到端参数估计，混合点估计 + MCMC CrI，40 次重复 95% CrI 覆盖率 0.938 ∈ [0.88, 0.99]，点估计误差 &lt; 2 后验 sd，中间产物可复算。

## [S1] Problem

仓库目前只有理论路线图 `docs/数学建模与参数优化理论知识路线图.md`，没有任何可运行代码。目标是：**按路线图逐节实现可独立运行、可客观判据自检、可提交的 C 语言实验 lab**，形成从地基层到里程碑项目的完整可复现实验体系。

用户约束（已确认）：

- 每一个小节 = 一个 lab；每个 lab 相对独立；**每完成一个 lab 提交一次**
- 语言：**C**（零第三方库依赖，核心算法自实现）
- 构建：本机 **MSYS2 UCRT64**，`gcc 16.1.0`（`C:\msys64\ucrt64\bin\gcc.exe`），`mingw32-make 4.4.1`
- 范围：**全路线 A→B→C→D + M1–M3**（B7/C9 选读章一并规划，实现时可标低优先级）
- 结构：**lab 自包含**：`src/` 业务算法 + `test/` 多场景测试 + `vendor/` 前序算法 copy
- 判据：**`bin/test` 自检 + 文本报告**（PASS/FAIL + 关键数据落盘）
- 分支：**直接在 master 上逐 lab 提交**

## [S2] Design

### 2.1 总体架构（当前实现）

**组织方式（已落地）**

- `src/`：**本 lab 知识点对应的业务算法实现**（学习主体；正本）
- `test/`：**测试** — `harness.c/h` + `test_main.c`（**不用 check.c 命名**）
- `vendor/`：**前序 lab 业务算法的 copy**（本 lab 编译使用；正本在前序 lab 的 `src/`）
- **无仓库级 `common/`**：算法正本只在各 lab `src/`

**测试 CLI**

```text
labs/<id>/bin/test                 # 全量：所有算法所有场景
labs/<id>/bin/test --list          # 列出 suite/case
labs/<id>/bin/test <suite>         # 单算法全部场景
labs/<id>/bin/test <suite>/<case>  # 单场景
mingw32-make check TEST_ARGS=<suite|suite/case>
```

每个业务算法 = 一个 **suite**；算法下多个 **场景 case**（正常路径 + 边界/失败/对照）。

```text
modeling-labs/
├── AGENTS.md / README.md / Makefile
├── scripts/sync_vendor.py    # 前序 lab src → 本 lab vendor
├── scripts/templates/        # harness 模板
├── docs/
├── mk/lab.mk                 # -Isrc -Ivendor -Itest；产出 bin/test
└── labs/<lab-id>/
    ├── src/                  # 本章算法（正本）
    ├── test/                 # harness + test_main
    ├── vendor/               # 前序算法 copy + VENDOR.md
    ├── README.md / report.md / results/
```

### 2.2 Lab 判定粒度与命名

| 层 | lab-id | 对应路线图节 |
|---|---|---|
| A | `A1-numeric-la` | A1 数值计算与线性代数 |
| A | `A2-prob-stat` | A2 概率与统计 |
| A | `A3-convex-opt` | A3 凸分析与优化理论 |
| B | `B1-line-search-gd` | B1 一维搜索与梯度法 |
| B | `B2-newton-quasi-tr` | B2 牛顿型、CG 与信赖域（含无导数） |
| B | `B3-least-squares` | B3 最小二乘与模型拟合 |
| B | `B4-lp-ip` | B4 线性与整数规划 |
| B | `B5-constrained` | B5 约束非线性优化 |
| B | `B6-sgd-adam` | B6 随机梯度与自适应优化 |
| B | `B7-prox-admm` | B7 不可微与近端方法（选读） |
| C | `C1-rng-mc` | C1 随机数生成与蒙特卡洛积分 |
| C | `C2-mcmc` | C2 采样算法与 MCMC |
| C | `C3-sa` | C3 模拟退火 |
| C | `C4-ts-vns-ils` | C4 禁忌搜索与 VNS/ILS |
| C | `C5-ga-de` | C5 进化算法：GA 与 DE |
| C | `C6-es-cmaes` | C6 进化策略与 CMA-ES |
| C | `C7-pso-aco` | C7 群智能：PSO 与 ACO |
| C | `C8-constraint-mo` | C8 约束处理与多目标 |
| C | `C9-bayes-opt` | C9 贝叶斯优化与代理模型（进阶选读） |
| D | `D1-interp-spline` | D1 插值与样条 |
| D | `D2-ode-sim` | D2 ODE 数值解与仿真模型 |
| D | `D3-graph-combo` | D3 图与组合优化 |
| M | `M1-curve-fitting` | M1 曲线拟合工作台 |
| M | `M2-global-bench` | M2 全局优化基准测试台 |
| M | `M3-e2e-param-est` | M3 端到端参数估计与不确定性量化 |

**一个小节 = 一个 lab = 一次 git commit**（实现 + 自检报告 + 本 lab 的 common 增量）。不在 lab 内部按实验编号拆 commit。

### 2.3 实现顺序（依赖拓扑 + 路线图推荐）

严格按「依赖不悬空」推进；里程碑在依赖齐备后做：

1. **脚手架**（首个 commit，非 lab）：根 Makefile、README、AGENTS.md、common 骨架、.gitignore、既有 docs 入库
2. A1 → A2 → A3
3. B1 → B2 → B3 → B4 → B5 → B6 →（B7 选读，默认排在 B 层末）
4. C1 → C2 → C3 → C4 → C5 → C6 → C7 → C8 →（C9 选读，默认排在 C 层末）
5. D1 → D2 → D3（D 层可与 C 后半并行规划，但仍串行提交）
6. M1（依赖 A1 B1 B2 B3）→ M2（依赖 C1–C7 + B2，可与 D 层穿插）→ M3（依赖 M1 M2 D2 C2，收口）

实现节奏：每 lab 一次会话内完成「`src/` 实现算法 → `vendor/` copy 前序依赖 → `test/` 多场景 → `make check` 过关 → report → commit」。

### 2.4 工具链与语言约定

| 项 | 约定 |
|---|---|
| 编译器 | `gcc`（MSYS2 UCRT64，PATH 已含） |
| 标准 | **C11**（`-std=c11`） |
| 警告 | `-Wall -Wextra -Wpedantic -Werror=implicit-function-declaration` |
| 优化 | 默认 `-O2` |
| 数学库 | 系统 `libm`（`-lm`）；**禁止**第三方数值库 |
| 构建 | `mingw32-make`；根 Makefile 统一入口 |
| 浮点 | 双精度 `double` 为主 |
| 命名 | 标识符英文 snake_case；报告与 lab README 用中文 |
| 随机种子 | 所有随机实验默认固定种子可复现 |
| I/O | 数据落盘到 `labs/<id>/results/` |

根 Make 目标：

```text
mingw32-make              # 构建全部 lab（src + test + vendor）
mingw32-make check        # 全部 lab 全量测试（bin/test）
mingw32-make check-A1     # 指定 lab 全量
mingw32-make lab-A1       # 仅构建指定 lab
mingw32-make vendor-sync  # 从前序 lab 的 src 刷新 vendor
mingw32-make clean
```

### 2.5 算法归属与 vendor（当前纪律）

- **算法正本**在各 lab 的 `src/`（按章节首次教学归属）
- **vendor** 是前序 lab `src/` 的 copy，本 lab **实际编译 vendor**
- **无 `common/`**；`scripts/sync_vendor.py` 维护 copy 依赖表
- 更新共享算法：改**归属 lab 的 `src/`** → `sync_vendor` 刷新依赖方 vendor → 同 commit
- 单 lab 专用算法只写在该 lab `src/`（如 box_qp、sgd、prox）；`knapsack`/`simplex`（B4）已被 D3 等 vendor，属共享正本

| 首次教学 lab | src 正本算法 | 典型 vendor 消费方 |
|---|---|---|
| A1 | vec, mat, linalg, numcal | A3, B* |
| A2 | dist, stats, gof, rng | A3, B*, C1 |
| A3 | conv | B2, B5 |
| B1 | opt, linesearch | B2, B5 |
| B2 | optcore, bench | B5 |
| B3 | lsq | （后续 M1） |
| B4 | simplex, knapsack | （D3 等） |
| B5 | nlp, box_qp | （C8/M*） |
| B6 | sgd | （M2） |
| B7 | prox | — |
| C1 | rng（深）, mc | C2–C9, M* |

| 阶段 | 新模块（写入**新 lab 的 src**，再 vendor） | 复用方 |
|---|---|---|
| C2 | mcmc（MH/Gibbs/R̂） | M3 |
| C3–C8 | meta / tsp | M2 |
| C9 | gp（选读） | — |
| D1–D3 | interp / ode / graph | M* |

### 2.6 目标函数与问题接口（跨 C/M 层）

为算法实验与 M2 测试台统一：

```c
typedef struct {
    int dim;
    double (*f)(const double *x, int n, void *ctx);
    void *ctx;
    /* 可选边界；无界则 lb/ub 为 NULL 或 ±inf */
    const double *lb, *ub;
} mlab_problem;
```

基准函数（Sphere / Rosenbrock / Rastrigin / Griewank / Ackley 等）集中在 `mlab_bench.h`，由 M2 统一使用；C 层 lab 先就地实现所需函数，M2 时收敛到 common。

### 2.7 Lab 测试与报告契约

每个 lab 提供：

```text
labs/<id>/src/*           # 本章业务算法
labs/<id>/test/harness.*  # 测试脚手架
labs/<id>/test/test_main.c# suite/case 注册与 CLI
labs/<id>/bin/test        # 测试入口；退出码 0 = 所选用例 PASS
labs/<id>/vendor/         # 前序算法 copy（若有）
labs/<id>/report.md       # 人读报告
labs/<id>/results/*       # 数据
```

测试约定：

- **suite** = 算法名；**case** = 场景（正常/边界/失败/对照），每算法至少 2 个场景
- CLI：全量 / `--list` / suite / `suite/case`
- 对路线图**客观过关判据**逐条断言，打印 `PASS` / `FAIL`
- 统计类判据用固定 seed + 足够重复；可注明精简模式
- 全量运行时可写 `report.md`

### 2.8 提交约定

```text
lab(A1): 一句话

- src: 本章算法文件
- vendor: 来自 <前序 lab>/src: ...
- test: suite/case 与判据
- 判据: <关键锚点>
```

- 仅 lab 完成且 `mingw32-make check-<id>` 通过后 commit
- 提交 `src/`、`test/`、`vendor/`、`report.md`、`results/`；不提交 `bin/`、`*.o`、`build/`

### 2.9 脚手架阶段交付（首个 commit，审核通过后立刻做）

| 项 | 内容 |
|---|---|
| `README.md` | 目标、工具链、`bin/test` 用法、lab 清单 |
| `AGENTS.md` | src/test/vendor 约定、CLI、算法归属表 |
| `.gitignore` | `build/`, `*.o`, `bin/` |
| 根 `Makefile` | `all/check/lab-%/check-%/vendor-sync` |
| docs | 路线图 + 本设计文档 |

### 2.10 各 Lab 实现要点与判据映射（实现清单）

下列为设计层任务清单：每项对应未来一个实现 commit。判据原文以路线图为准，本节只固定**实现范围与验收锚点**。

#### A 层

**A1-numeric-la**
- common: vec/mat/范数；高斯消元 + 部分主元 LU；Cholesky；数值微分；复化 Simpson
- 实验：①可控条件数矩阵上 LU 残差 ②差分 U 形曲线与最优步长 ③Simpson 收敛阶 ④Cholesky 自检
- 判据锚点：残差与 κ 尺度一致；最优步长/√ε ∈ [0.3, 3]；Simpson 阶 4±0.3

**A2-prob-stat**
- common: 分布、矩、KS（含临界值）、卡方、CLT 直方图统计工具
- 实验：①CLT 验证 + 卡方 ②KS 检验 1000 次重复拒绝率 ③控制变量法方差缩减
- 判据锚点：KS 拒绝率 ∈ [0.03, 0.07]（α=0.05）；方差缩减比相对误差 < 10%

**A3-convex-opt**
- 实验偏理论验证：①最速下降收敛因子 vs (κ−1)/(κ+1) ②双谷函数多起点局部法 ③简单 QP 的 KKT 乘子恢复
- 判据锚点：收敛因子相对误差 < 20%（≥3 个 κ）；局部法全局命中率 < 100% 且可复现

#### B 层

**B1-line-search-gd**
- 黄金分割；Armijo；最速下降；病态二次问题轨迹落盘
- 判据锚点：收缩比 ≈ 0.618（误差 < 1%）；病态/良态迭代比与 κ 同数量级

**B2-newton-quasi-tr**
- 牛顿（Hessian 正定化）；BFGS（+L-BFGS 接口预留）；线性 CG；信赖域 Dogleg；坐标下降；Nelder-Mead；（地图）DIRECT 注记
- 判据锚点：CG ≤ n 步残差 < 1e-10；牛顿阶 2±0.3；BFGS > 1.2；Rosenbrock 上 BFGS 评估数 ≥5× 优于最速下降；信赖域病态起点失败率更低；噪声场景 NM 相对梯度法韧性
- common: `mlab_opt.h` 问题接口 + 收敛阶测量（回收 A3）

**B3-least-squares**
- 线性 LS：正规方程 vs QR；LM；加权/Huber（基础）；CI 与覆盖率
- 判据锚点：参数恢复在 3σ 内；95% CI 覆盖率 ∈ [88%, 99%]（500 次）；LM 在记录的初值邻域内成功率 100%

**B4-lp-ip**
- 修订单纯形 + Bland；两阶段；对偶/互补松弛；分支定界
- 判据锚点：原对偶间隙 = 0（双精度）；BnB vs 暴力 100/100 一致

**B5-constrained**
- 外点罚扫描；增广拉格朗日；投影梯度 vs 有效集
- 判据锚点：AL 同精度所需罚系数 ≤ 纯罚 1%；KKT 残差 < 1e-6；投影梯度与有效集 100 题一致

**B6-sgd-adam**
- SGD/递减步长/动量/Nesterov/AdaGrad/RMSProp/Adam/AdamW；调度
- 判据锚点：log-log 斜率 −0.5±0.1；Nesterov 迭代显著更少；Adam 步长鲁棒区间更宽；纵向缩放不变性定量对照

**B7-prox-admm**（选读，排 B 末）
- 次梯度；ISTA/FISTA；ADMM（与 B5 对账）
- 判据锚点：LASSO 支持恢复 ≥90%；FISTA 迭代 ≤ ISTA 1/2；ADMM vs AL 目标相对差 < 1e-8

#### C 层（重点）

**C1-rng-mc**
- common: RNG（深实现 xorshift* 或 MT32 精简版，LCG 作对照）、Box-Muller、KS 复用、MC 积分、拒绝采样、重要性采样
- 实验：质量测试套件；逆变换；拒绝采样接受率；MC 斜率与方差对比
- 判据锚点：卡方/自相关过关；KS 拒绝率区间；MC 斜率 −0.5±0.05；拒绝采样接受率误差 < 2%

**C2-mcmc**
- MH、Gibbs、多链 R̂、自相关/thin
- 判据锚点：2D 高斯均值/协方差对账；Gibbs ACT < MH 的 1/3；burn-in 后 R̂ < 1.1

**C3-sa**（重点）
- Metropolis 用于目标；几何降温；邻域（连续扰动 / TSP swap+2-opt）；初始温度标定；接受率诊断
- 实验：Rastrigin 1000 次命中率；参数扫描 PGM；TSP 邻域对比；接受率曲线
- 判据锚点：调参后命中率 ≥80%；热图可复算波动 <±5pp；2-opt 显著优于 swap（p<0.01）

**C4-ts-vns-ils**
- TS（禁忌任期/特赦）；VNS/VND；ILS；四机制对账表
- 判据锚点：禁忌长度 U 形；VNS 多邻域显著更优；ILS 扰动 U 形；对账表完整可归因

**C5-ga-de**
- GA 编码/选择/交叉/变异/精英；DE 变体与 F×CR 网格；多样性诊断
- 判据锚点：F×CR 全表落盘（每格 ≥50 次）；rand/1 vs best/1 优势相反（p<0.01）；早熟多样性更早跌落

**C6-es-cmaes**
- (1+1)-ES + 1/5 规则；CMA-ES 核心（简化但可运行：路径、秩-μ、σ）；与 DE 对照；协方差主轴对齐 PGM
- 判据锚点：成功率 → 0.2±0.05；Rosenbrock 上 CMA 评估数显著优于 DE；主轴夹角收敛可复现

**C7-pso-aco**（重点）
- PSO 三分量、w 扫描、拓扑、消融；ACO + TSP；与 SA 对账
- 判据锚点：w×拓扑热图完整；完整版显著优于单项消融；ACO+2opt 相对贪心 gap 可解释、与 SA 对比显著

**C8-constraint-mo**
- Deb 可行性法则 vs 死亡惩罚 DE；NSGA-II；HV/IGD；排序翻转反例
- 判据锚点：成功率差 ≥20pp；ZDT1 IGD < 1e-2；端点误差 < 5%

**C9-bayes-opt**（进阶选读）
- GP（Cholesky 复用）；EI；BO 循环；与随机/LHS 对比
- 判据锚点：2σ 覆盖率 [90%, 98%]；EI-BO 评估数 ≤ 随机 1/3（p<0.01）

#### D 层

**D1-interp-spline**
- Runge 现象；三次样条自然边界；RBF 形状参数扫描
- 判据锚点：样条阶 4±0.3；最优形状参数区残差 < 1e-6

**D2-ode-sim**
- Euler/RK4/稳定性/RKF45 自适应；模板：谐振子、Lotka-Volterra、（报告可扩展 SEIR）
- 判据锚点：RK4 阶 4±0.3；能量漂移 < 1e-6（对照 Euler）；自适应局部误差 ≤ 2·tol（100 初值）

**D3-graph-combo**
- Dijkstra/Floyd 互验；2-opt；Held-Karp n≤20；贪心基线
- 判据锚点：100/100 最短路一致；Held-Karp 下启发式 gap 可排序稳定；2opt 显著优于贪心

#### M 层

**M1-curve-fitting**
- 合成数据 + BFGS/LM 双引擎 + CI 覆盖率 + 自动报告
- 判据锚点：CI 覆盖率区间；双引擎收敛域内成功率 100%；σ 扫描误差线性放大

**M2-global-bench**
- 统一 bench 库 + 算法接口（SA/GA/DE/PSO，可扩 CMA）+ 混合全局-局部框架 + 报告表
- 判据锚点：4×5×3 全表每格 ≥100 次；混合框架多峰显著更优；《算法选择参考表》有本轮数据

**M3-e2e-param-est**
- 选仿真模型（设计默认：**Lotka-Volterra**，代码结构预留 SEIR/PK）→ 合成观测 → M2 点估计 → C2 MCMC → 可信区间对账 → 一键报告
- 判据锚点：95% CrI 真值覆盖 ∈ [88%, 99%]；点估计误差 < 2 后验标准差；全流程脚本贯通

### 2.11 每 Lab 交付物验收（通用）

1. `labs/<id>/` 含 README + `src/` + `test/` + Makefile（+ `vendor/` 若有依赖）
2. `src/` 有本章业务算法源文件
3. `bin/test --list` 可列出 suite/多场景 case
4. `mingw32-make check-<id>` 退出码 0
5. `report.md` 含实测与 PASS 表
6. vendor 正本变更与 copy 同 commit
7. 一次符合消息约定的 commit

### 2.12 风险与对策

| 风险 | 对策 |
|---|---|
| 统计判据运行过久 | `QUICK` 精简模式；正式报告默认完整模式；种子固定 |
| 病态矩阵/收敛阶对实现细节敏感 | 报告记录算法变体与参数；判据容差已按路线图 |
| CMA-ES/NSGA-II 实现量大 | 先最小可运行核心 + 路线图判据；不追求工业级全特性 |
| Windows 路径/可执行后缀 | Makefile 使用 `$(EXEEXT)`；路径统一正斜杠或调用层处理 |
| 空仓直接改 master | 用户已明确同意；不建 worktree |
| 选读章拖慢主线 | B7/C9 标低优先级，主线过完后补；设计不删除 |
| common 与 vendor 漂移 | 算法正本在归属 lab 的 src；依赖方 vendor 必须 sync 后同 commit；check 编译 vendor |

## [S3] Out of Scope

- 不引入第三方数值/优化/绘图库（GNUplot、BLAS、C++ 库等）
- 不做 GUI；热图/曲线以 PGM + 文本数据为主（用户可在外部查看）
- 不做 GPU/并行（OpenMP 可选注记，非交付要求）
- 不把本设计写成教材正文；知识点以实验与报告中的对照结论呈现
- 不自动 merge 到其他分支、不自动 push 远端（仓库尚无远端约定）
- 不在未审核本设计前实现任何 lab 业务代码

## Tasks

脚手架与实现任务（用户批准本设计后按序执行；勾选状态在实现过程中更新）：

- [x] T0: 脚手架入库 — acceptance: README/AGENTS/.gitignore/根 Makefile/common 骨架存在；docs 已提交；`mingw32-make` 可空构建通过 (covers: S2.9)
- [x] T1: A1-numeric-la — acceptance: `make check-A1` 退出 0；report 判据与路线图 A1 锚点一致 (covers: S2.10-A1; depends: T0)
- [x] T2: A2-prob-stat — acceptance: KS 拒绝率与方差缩减判据 PASS (covers: S2.10-A2; depends: T1)
- [x] T3: A3-convex-opt — acceptance: 收敛因子与多起点非凸实证 PASS (covers: S2.10-A3; depends: T1)
- [x] T4: B1-line-search-gd — acceptance: 黄金分割与病态 GD 判据 PASS (covers: S2.10-B1; depends: T1,T3)
- [x] T5: B2-newton-quasi-tr — acceptance: CG/收敛阶/BFGS 对比/信赖域/NM 判据 PASS (covers: S2.10-B2; depends: T4)
- [x] T6: B3-least-squares — acceptance: 3σ 恢复与 CI 覆盖率 PASS (covers: S2.10-B3; depends: T5,T2)
- [x] T7: B4-lp-ip — acceptance: 单纯形对偶间隙与 BnB 100 题对账 PASS (covers: S2.10-B4; depends: T1,T3)
- [x] T8: B5-constrained — acceptance: AL 罚量级优势与投影梯度互验 PASS (covers: S2.10-B5; depends: T5)
- [x] T9: B6-sgd-adam — acceptance: 步长调度/Nesterov/Adam 鲁棒性判据 PASS (covers: S2.10-B6; depends: T4,T2,T3)
- [x] T10: B7-prox-admm（选读） — acceptance: ISTA/FISTA/ADMM 判据 PASS (covers: S2.10-B7; depends: T5)
- [x] T11: C1-rng-mc — acceptance: RNG 质量、KS、MC 斜率、拒绝采样判据 PASS (covers: S2.10-C1; depends: T2)
- [x] T12: C2-mcmc — acceptance: MH/Gibbs/R̂ 判据 PASS (covers: S2.10-C2; depends: T11)
- [x] T13: C3-sa — acceptance: Rastrigin 命中率、热图、TSP 邻域对比 PASS (covers: S2.10-C3; depends: T12,T4)
- [x] T14: C4-ts-vns-ils — acceptance: U 形与四机制对账 PASS (covers: S2.10-C4; depends: T13)
- [x] T15: C5-ga-de — acceptance: F×CR 表、变体对比、多样性诊断 PASS (covers: S2.10-C5; depends: T13)
- [x] T16: C6-es-cmaes — acceptance: 1/5 规则与 CMA vs DE 判据 PASS (covers: S2.10-C6; depends: T13)
- [x] T17: C7-pso-aco — acceptance: w×拓扑热图、消融、ACO 对账 PASS (covers: S2.10-C7; depends: T13)
- [x] T18: C8-constraint-mo — acceptance: 可行性法则优势与 NSGA-II/IGD PASS (covers: S2.10-C8; depends: T15,T8)
- [x] T19: C9-bayes-opt（选读） — acceptance: GP 覆盖率与 EI-BO 效率 PASS (covers: S2.10-C9; depends: T11,T5)
- [x] T20: D1-interp-spline — acceptance: 样条阶与 RBF 判据 PASS (covers: S2.10-D1; depends: T1)
- [x] T21: D2-ode-sim — acceptance: RK4 阶/能量漂移/自适应误差 PASS (covers: S2.10-D2; depends: T1,T6)
- [x] T22: D3-graph-combo — acceptance: 最短路互验与 Held-Karp gap PASS (covers: S2.10-D3; depends: T1,T7)
- [x] T23: M1-curve-fitting — acceptance: 双引擎拟合 + 覆盖率报告 PASS (covers: S2.10-M1; depends: T6)
- [x] T24: M2-global-bench — acceptance: 算法×函数×维 全表与混合框架判据 PASS (covers: S2.10-M2; depends: T13,T15,T16,T17,T5)
- [x] T25: M3-e2e-param-est — acceptance: 端到端估计与 CrI 覆盖率 PASS，报告贯通 (covers: S2.10-M3; depends: T23,T24,T21,T12)

设计文档本身在用户批准前不勾选实现任务；批准后从 T0 开始，并将 `status` 改为 `in-progress`。

## 3. 修复轮（2026-09，全仓库审查后）

对 26 个 lab 做路线图符合性审查后，按 9 批修复：

- **R0** 文档：AGENTS.md 归属表补 C2/C3/C4、去重 M3；本节建立。
- **R1** A 层：A1 去除 report.md 自动覆写、判据口径对齐路线图、恢复 results 落盘、新增迭代法（Jacobi/GS/SOR）；A2 补 MLE/CI 与其余三种方差缩减、CLT 直方图卡方、KS 用正本 API 且 1000 次 [0.03,0.07]；A3 收敛因子改误差范数比口径、KKT 乘子恢复数值化。
- **R2** B1–B3：B1 补收缩比判据与 Wolfe；B2 收敛阶真测、fevals 真实计数、信赖域 10 起点、NM 噪声实验，新增 L-BFGS/DFP/坐标下降/Hooke-Jeeves/DIRECT；B3 补病态 QR vs 正规方程、3σ 恢复、LM 初值扫描、加权/Huber。
- **R3** B4–B7：B4 Phase II 禁人工列、对偶符号修正与对偶/互补松弛测试、100 实例对账、顶点枚举互验，新增指派与 Gomory 割演示；B5 真 KKT 残差（含乘子恢复）、投影梯度回溯保护、罚系数扫描、100 箱式问题，新增内点法/SQP；B6 mini-batch 随机化 + 实验 1/2/4，新增 SVRG；B7 新增次梯度法与近端点法/基追踪，ADMM 与 B5 AL 带约束 QP 对账 <1e-8。
- **R4** C1–C4：C1 补 Box-Muller/Marsaglia 测试、离散逆变换、间隔检验、IS 诊断、Sobol 低维；C2 补 HMC；C3 接通 T0 标定实验、补重加热/对数调度；C4 修局部搜索预算语义（同预算对账失真 +55~69%）、补 LNS/GRASP/长期记忆。
  - R4 完成（2026-09-19）：C1 判据全量收紧至路线图口径（KS 1000 次 [0.03,0.07]、ACF 2/√N、斜率 −0.5±0.05），Sobol 与 scipy.stats.qmc 逐点一致；C2 HMC d=16 IAT 比 0.024；C3 多重启 best-of-8 命中 1.000 vs 单跑 0.590，对数调度/周期重加热的局限如实呈现；C4 预算语义修复后 VNS/ILS fevals 恰好收敛到预算值，VND 重排序（2opt→oropt→swap）消除稀释，多邻域 p=1.2e-10。全量 sync_vendor 一次性消除 R1–R3 累积的下游 vendor 漂移（110 文件），下游 11 lab 编译复验通过；C5–M3 的 results 复算随各自批次（R5–R7）执行。
- **R5** C5 正本修复（DE 父代缓存、GA 真 top-k 精英）→ sync_vendor 级联 C6/C7/C8 重跑；C5 补 Rosenbrock 单峰对比、report 结论对齐数据；C6 补 comma/plus 与 IPOP；C7 ρ 扫描重设计（原数据无区分度）、补 Clerc/VN/MMAS；C8 补修复法/自适应罚/MOEA-D。
- **R6** C9/D1–D3：C9 覆盖率双口径、TPE/批量 BO；D1 TPS 多项式增广、kriging 对照；D2 补药代动力学房室模板、RKF45 对精确解判据；D3 补 BFS/DFS 测试、盆地分布、最大流 Edmonds-Karp。
- **R7** M 层：M1 初值 ±20% 与 t 分位 CI；M2 DE 预算标定修正并重跑全表、补收敛曲线；M3 防御性修复（xbest/SEIR guard/产物覆盖）。
  - R7 完成（2026-09-20）：M1 初值真 ±20% +「引擎收敛且参数恢复」双口径 30/30，CI 改 t(28) 分位（不完全 Beta 求逆，与标准表误差 <5e-5，覆盖率 95.4%/94.4% 仍 ∈ [88,99]），fevals 去双计；收敛阈值按双精度分辨极限回调（LM 1e-10、BFGS 1e-8），消除 B3 R3 停滞返回码后的假性失败。M2 混合框架等预算对齐（总评估 = 单层同口径，n_eval 真实计数，精修改 Hooke-Jeeves：等预算下探针 6 变体对比全面占优），DE 预算标定重验（旧正本整群重评估实际耗 1780 vs 名义 900，修后恰好 900）并重跑 60 格主表，新增 6 算法×5 函数×9 检查点收敛曲线 CSV（同 seed 独立重跑法，非前缀假设），热图批量化至 5 函数并去条件偏差（成功率分母改 n_run），GA 子代数钳制到剩余预算，选择表尾注改为数据背书（Rosenbrock 实测 HYBRID/CMA-ES 均 0%，旧无数据断言删除；Rastrigin HYBRID 97%）；Griewank 上 GA 77/100 反超 HYBRID 70/100 如实呈现。M3 xbest 初始化 + 全起点失败返回错误、早停条件改为对比更新前历史最优、nll 按 model_id 分派（SEIR 不再静默用 LV 似然）、单链 R̂ 落 NA、repro 产物独立目录不覆盖主管线产物，判据 2 逐 rep 报告 err<2sd 占比 155/160=0.969（反例 rep13/26/30/36 逐字段可查）。
- **R8** 全量 25 lab `mingw32-make check` 复验与 README/report/results 终检。
  - R8 完成（2026-09-20）：`mingw32-make check` 25 lab 全绿（退出码 0，逐 lab Summary 全 PASS）；`bin/test --list` 逐 lab 验证通过（25/25）；终检确认每个 README 含知识点/实验/判据锚点/用法，每个 report 含环境/实测/PASS 表与总判定；全量复跑后 results 零漂移（工作树 clean），results 均可由现行代码复算。至此修复轮 R0–R8 全部完成，路线图 6 路审查提出的各批次 P1/P2 修复项已全部落地并逐批提交。
