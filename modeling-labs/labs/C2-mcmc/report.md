# C2-mcmc 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — mcmc（RWM-MH / 独立 MH / HMC(leapfrog) / 顺序 Gibbs / IAT / Gelman-Rubin R̂ / 共轭 BVN 后验）
- vendor: A2(dist/stats/gof) + C1(rng/mc)；主 RNG splitmix64
- seed: mh=6201/6202/6220/6221, gibbs=6203/6210/6211, hmc=6501/6502, rhat=6300..6303, ref=6401

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-mh-gaussian-moments | 2D 高斯均值误差 < 3SE；协方差元素相对误差 < 10% | 独立高斯: m=(1.0109,-0.5190)，err=(0.0109,0.0190) < 3SE=(0.090,0.030)；cov 对角 rel 0.1%/0.7%，非对角 \|0.0227\|/(1.5·0.8)=1.9% | PASS |
| E2-gibbs-vs-mh-iat | Gibbs IAT / RWM-MH IAT < 1/3（同目标） | ρ=0.95: τ_G=20.70, τ_MH=224.54, ratio=0.0922 | PASS |
| E3-gibbs-conjugate | 共轭二元正态后验样本矩 vs 解析值 | post μ=(0.9516,-0.6615), s=(0.1578,0.1578), ρ=0.7992；Gibbs mean 对齐 3SE 内；cov12=0.02022 vs 解析 0.01989 | PASS |
| E4-gelman-rubin | burn-in 后 R̂ < 1.1 且持续（4 链） | 后验段 max R̂=1.0199→1.0027，全程 < 1.1 | PASS |
| E5-thin | thin 降低存盘样本自相关 | thin=1: acf1=0.987, τ=186.5；thin=80: acf1=0.425, τ=2.7 | PASS |
| E6-hmc-note | HMC（注记项）：d=16 下 IAT 显著短于 RWM | iat rwm=41.30 hmc=1.00（ratio 0.024 < 0.5），acc_hmc=0.937，mean0 均在 3SE 内 | PASS |
| 参照 | 独立 MH（提议≈目标）IAT ≈ 1 | iat=(1.01,1.07)，acc=1.000 | PASS |

## 关键数据

- E1/E2/薄化: `results/mh_gaussian.csv`、`results/thin_acf.csv`
- E2/E3 IAT: `results/gibbs_mh_iat.csv`（ρ=0.95 下 Gibbs 有效样本约 2898 vs MH 267）
- E4 R̂ 轨迹: `results/rhat_trace.csv`
- E6 HMC 对照: `results/hmc_vs_mh.csv`
- 高斯 RWM 对 ρ=0.95 的接受率 0.665 但 IAT~225：沿相关脊的随机游走混合慢，印证 thin / 结构化核（Gibbs）必要性。
- HMC 注记项落地：leapfrog 20 步、步长 0.5，d=16 对角高斯上近乎独立采样（τ=1.0），
  同链长 RWM τ=41.3——梯度引导提议的高维优势落到数据。

## 结论与备注

- 未归一化目标下 MH/Gibbs 用密度比采样，避开配分函数。
- 顺序 Gibbs（x2|x1 新）才能恢复相关结构；同步更新会把经验协方差打散（实现时曾踩坑，已修正）。
- 共轭正态后验闭式可作 MCMC 矩对账的金标准，供 M3 复用。
- common 增量: `mlab_mcmc`（链容器、RWM/独立 MH、HMC、BVN Gibbs、IAT、R̂、共轭后验）。

## 总判定

- PASS: 8 / FAIL: 0 — **PASS**
