# M3-e2e-param-est 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — e2e（LV 仿真 + 似然 + M2 式混合点估计 + C2 MCMC + CrI + 报告）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + A3(conv) + B1(opt/linesearch) + B2(optcore/bench) + C1(rng) + C2(mcmc) + C5(de) + D2(ode)
- 模型: Lotka-Volterra θ=(α,β,γ,δ)=(1, 0.08, 0.6, 0.04)，y0=(12,4)，t∈[0,18]，n_obs=25，σ=0.7
- seed: 流水线 300101/301001/302001；点估计 310101；mcmc 320101/321001/322001；覆盖率逐次 370000+r·31 / 360000+r·19；可复算 380101

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-pipeline | 数据→报告一次贯通，中间产物落盘 | obs/theta/samples/report 均写出；θ̂=(0.9975,0.0796,0.5962,0.0403)；n_eval=2577（C5 修后口径真实计数） | PASS |
| E2-coverage | 95% CrI 真值覆盖率 ∈ **[88%, 99%]** | 40 次重复，平均 **0.938**（0.975/0.925/0.900/0.950），联合 0.850 | PASS |
| E2b-point-2sd-40rep | 点估计误差 **< 2×**后验 sd，逐 rep 报告（R7 扩展口径） | 40 rep × 4 参数中 **155/160=0.969** 达标；反例 rep13/rep26/rep30/rep36 如实呈现（rep36：β err/sd=3.0、γ 也越限，逐字段见 CSV） | PASS |
| E3-point-2sd | 主线用例点估计误差 **&lt; 2×**后验 sd | 4/4 参数 \|err\|/sd=(1.10,1.62,1.24,1.67) | PASS |
| E4-rhat | 多链 R̂ **&lt; 1.1** 且 CrI 含真值 | R̂=**1.025**，CrI hits=**4/4**，acc=0.209 | PASS |
| E5-repro | 同种子中间产物可复算 | max\|Δθ̂\|=**0**，max\|ΔCrI\|=**0**；repro 产物独立写入 `results/repro/`，不再覆盖主管线产物 | PASS |

## 点估计质量（场景样例）

| 用例 | θ̂ vs 真值 | 备注 |
|---|---|---|
| hybrid_recovers | max rel err **2.8%** | budget=800，多起点混合 |
| pipeline | 相对误差 &lt; 0.5% | budget=900 + MCMC |
| coverage 平均 | CrI 覆盖 93.8% | 每次 point_est budget=700 + MCMC burn=1000/keep=2000 |

## 关键数据

- 全流程报告: `results/m3_report.md`
- 参数表: `results/m3_theta.csv`
- 观测: `results/m3_obs.csv`
- 后验样本: `results/m3_mcmc_samples.csv`
- 覆盖率: `results/m3_coverage.csv`（含逐 rep ok0–ok3/max_ratio 列；单链 R̂ 落 NA）
- repro 产物: `results/repro/`（独立文件名，不覆盖主管线产物）

## 结论与备注

- **流水线**：`mlab_e2e_pipeline` 串起 synth → `mlab_e2e_point_est`（DE rand/1 → 缩小盒 DE best/1 → Nelder-Mead，**3 次多起点**取最优 NLL）→ `mlab_e2e_mcmc`（C2 `mlab_mh_rwm`，参数缩放坐标 + pilot 步长标定）→ CrI 分位数 → 中文报告。
- **局部模**：单次全局搜索偶发落入错误盆地（γ 顶到边界）；多起点显著稳定点估计，是覆盖率回到标称附近的关键。
- **R7 防呆修复**：① `mlab_e2e_point_est` 的 xbest 现已初始化，全部起点失败时返回错误而非用未初始化内存冒充成功；早停条件改为对比更新前历史最优（旧条件在本次即最优时恒真，第 2 次内点即停，多起点鲁棒性名存实亡）；NM 盒裁剪无改进时回退精修前点，保持 x 与 fcur 一致。② `mlab_e2e_nll` 按 model_id 分派仿真正本，SEIR 配置不再静默套用 LV 似然（估计主线仍为 LV，路线图「三选一」口径不变）。③ repro 用例产物写入独立子目录，不再覆盖 pipeline 产物。④ 单链（覆盖率实验）R̂ 无定义，theta CSV/报告/覆盖率 CSV 统一落 NA。⑤ 评估数随 C5 DE 修后口径重跑刷新（pipeline n_eval=2577 为真实计数）。
- **覆盖率**：40 次重复下平均 95% CrI 覆盖 **0.938**，落在 [0.88, 0.99]；边际覆盖接近标称 0.95，说明「均匀盒先验 + 高斯似然 + RWM」在本设定下频率学性质合理。
- **判据 2 逐 rep 口径（R7）**：40 rep × 4 参数中 155 个点估计误差 < 2×后验 sd（占 96.9%，接近标称 95%）；4 个反例 rep13/rep26/rep30/rep36 在 CSV 中逐字段可查（如 rep36 的 β：hat=0.0828 vs true=0.08，err/sd=3.0）——单次 4/4 全过（E3）与批量 96.9% 达标并不矛盾，后者才是判据的统计口径。
- **不确定性量化**：主线用例点估计误差均 &lt; 2 后验 sd；后验 sd 与噪声水平、观测数匹配（α 相对更准，δ 绝对 sd 最小）。
- **可复算**：固定 seed 下两次 pipeline 的 θ̂ 与 CrI 逐位一致（修复了缩小盒 DE 种群初始化越界读之后）。
- **SEIR 预留**：`mlab_e2e_seir_simulate` 走 D2 模板；估计主线与判据实验用 LV（路线图默认）；nll 分派保证 SEIR 配置不会被静默错误处理。
- **测试规模**：全量约数分钟（含 40 次覆盖率）；单 suite 可用 `TEST_ARGS=` 过滤。
- common 增量: `mlab_e2e_*`（配置/仿真/似然(model_id 分派)/混合点估计(防呆)/MCMC/CrI/流水线/报告）。

## 总判定

- PASS: 6 / FAIL: 0 — **PASS**
