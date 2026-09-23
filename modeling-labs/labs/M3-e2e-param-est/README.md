# M3 端到端参数估计与不确定性量化

完整建模闭环：仿真模型（Lotka-Volterra）→ 合成观测 → M2 式混合点估计 → C2 MCMC 后验 → 95% CrI → 一键报告。

## 知识点

- 机理模型 + 未知参数 + 噪声观测的参数估计形态
- 似然：高斯观测 + ODE 数值轨迹；先验：均匀盒
- 点估计：全局 DE + 缩小盒 DE + Nelder-Mead（M2 混合框架，多起点抗局部模）
- 不确定性量化：RWM-MH 后验样本 → 边际 95% CrI；Gelman-Rubin R̂ 诊断
- 频率学覆盖：重复实验下 Bayesian CrI 对真值的覆盖率
- SEIR 仿真接口预留（`model_id`），估计主线用 LV

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 流水线 | 合成→点估计→MCMC→CrI→报告，中间产物落盘 |
| E2 | CrI 覆盖 | 重复实验 95% CrI 真值覆盖率 ∈ **[88%, 99%]** |
| E3 | 点估计 | 误差 **&lt; 2 倍**后验标准差 |
| E4 | 后验诊断 | 多链 **R̂ &lt; 1.1**，CrI 含真值 |
| E5 | 可复算 | 同种子重跑 θ̂ / CrI 一致 |

路线图原文锚点：

- 95% 可信区间真值覆盖率 ∈ [88%, 99%]（多次重复实验）
- 点估计误差小于后验标准差的 2 倍
- 数据→报告全流程一次脚本贯通，中间产物可复算

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | e2e（LV/SEIR 仿真 + 似然 + 混合点估计 + MCMC + 流水线 + 报告） |
| `test/` | harness + test_main |
| `vendor/` | A1–A3, B1–B2, C1–C2, C5(de), D2(ode) |

测试 suite：pipeline、point_est、mcmc、coverage、report

```bat
mingw32-make check-M3
bin\test.exe --list
bin\test.exe pipeline
bin\test.exe coverage
bin\test.exe mcmc/posterior_cri_and_rhat
mingw32-make -C labs/M3-e2e-param-est check TEST_ARGS=<suite|suite/case>
```

## 中间产物

- `results/m3_obs.csv` — 观测与真轨迹
- `results/m3_theta.csv` — 真值 / 点估计 / 后验均值·sd / CrI（单链时 R̂ 落 NA）
- `results/m3_mcmc_samples.csv` — 后验样本（降采样）
- `results/m3_coverage.csv` — 覆盖率逐次实验（含逐 rep err<2sd 标记）
- `results/m3_report.md` — 全流程报告
- `results/repro/` — 可复算用例的独立产物（不覆盖主管线产物）
