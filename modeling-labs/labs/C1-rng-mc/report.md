# C1-rng-mc 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)；仅链接 libm
- 库: mlab 0.1.0 — rng（splitmix64/xorshift64*/LCG/MT19937 + 质量工具 + Sobol）+ mc（积分/拒绝/IS 权重诊断）
- 主发生器: splitmix64（与 A2 兼容）；对照: xorshift64* / MT19937 / LCG-MMIX
- 全部实验固定 seed，可由 `bin/test` 逐 case 复算

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-rng-quality | 卡方拒绝率在理论区间；滞后自相关 \|ρ\| < 2/√N | 四发生器 chi2 拒绝率 0.043~0.053；n=50000、lag≤40：acf1 最大 0.0078 < 0.00894，超界比例 ≤5%（≤15% 容限） | PASS |
| E1b-normal-sampling | Box-Muller / Marsaglia 矩与 KS | BM: mean=0.0055 sd=0.9993 skew=-0.017 kurt=-0.004 KS=0.0055；极法: 0.0090/1.0088/0.005/-0.017 KS=0.0064（crit 0.0068） | PASS |
| E2-ks-inverse-transform | 逆变换 KS 拒绝率 ∈[0.03,0.07]（1000 次） | Exp=0.049 Normal=0.041；离散逆变换 chi2 拒绝率=0.067 | PASS |
| E2b-gap-test | 间隔检验（截尾几何 GOF） | α=0.3、k=5、300 次：拒绝率=0.067 ∈[0.03,0.07] | PASS |
| E3-rejection-accept-rate | 接受率 vs 理论 1/c 相对误差 <2% | emp=0.59684 theory=0.59814 rel=0.22% | PASS |
| E4-mc-slope-and-is | MC 斜率 −0.5±0.05；IS 方差更小 | slope_pi=-0.519 slope_gauss=-0.546（N=256..32768，各点 40 次去重叠种子）；var_is/var_plain=0.279 | PASS |
| E4b-is-weight-diag | 权重方差与坏比值诊断 | 好提议 q∝e^{x/2}: ESS/n=0.979 cv²=0.021 maxw=6.5e-5；坏提议 q=N(0,0.5²): ESS/n=0.053 cv²=17.7 maxw=0.023 → 诊断报警 | PASS |
| E5-sobol-note | Sobol 注记项：与参考一致；低差异 vs 伪随机 | 前 16 点与 scipy.stats.qmc.Sobol 逐点一致（max diff=0）；N=4096 积分误差 2.3e-4 vs MC 1.2e-2（约 1/52），全档 < 0.5×MC | PASS |

## 关键数据

- E1: 600 次卡方均匀性（n=4000,k=16）+ n=50000 自相关（lag≤40，2/√N=0.00894）。见 `results/rng_quality.csv`。
- E1b: n=40000 正态矩（|mean|<0.02、|sd−1|<0.02、|skew|<0.05、|kurt−3|<0.10）+ 单次 KS。见 `results/normal_quality.csv`。
- E2: 指数/正态各 1000 次 KS（n=200，crit=0.0960）；离散逆变换 300 次×n=600。见 `results/ks_inverse.csv`。
- E2b: 间隔检验 300 次×n=40000。见 `results/rng_quality.csv` 同目录运行日志（case 详情）。
- E3: 目标 φ(x) 截断至 [-2,2]，均匀包络；c=4φ(0)≈1.595769。见 `results/rejection_rate.csv`。
- E4: π 命中法与 ∫e^{-x²}；N=256·2^k（k=0..7），每点 40 次，种子格点距 1024 去重叠。见 `results/mc_slope.csv`。
- E4b: 自归一化 IS + 权重 w=p/q 诊断（ESS=(Σw)²/Σw²、cv²、max w/Σw）。见 `results/is_weight_diag.csv`、`is_variance.csv`。
- E5: Sobol 方向数取 Sobol'-Levitan 标准表（灰码变换等效形式，20 bit，dim≤8，周期 2^20）。见 `results/sobol_convergence.csv`。

## 结论与备注

- splitmix64 为全路线默认 RNG；MT19937 扭转递推与标准一致，但播种用 splitmix 填充（与标准 init_genrand 序列不同，见 `src/rng.c` 注记），只影响复现性不影响质量。
- LCG 仅作对照，同样通过卡方与自相关口径。
- IS 权重诊断把"坏比值"量化：窄提议下 ESS/n 从 0.98 崩到 0.05，单权重占比放大 350 倍——提议失配可直接从诊断量读出。
- Sobol（注记项）实测：同 N 下积分误差约为伪随机 MC 的 1/50，验证低差异序列 O((log N)^d/N) 对 O(1/√N) 的优势；高维表与 32 bit 方向数不在本 lab 范围。
- common 增量: `mlab_rng` 多发生器+质量工具（含 gap test、离散逆变换）、`mlab_sobol_point`、`mlab_mc` 积分/拒绝/IS（含 `mlab_mc_is_weighted` 权重诊断）。

## 总判定

- PASS: 18 / FAIL: 0 — **PASS**
