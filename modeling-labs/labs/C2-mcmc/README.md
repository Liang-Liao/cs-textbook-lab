# C2 采样算法与 MCMC

Metropolis-Hastings、Gibbs、链诊断（IAT / Gelman-Rubin R̂）。

## 知识点

- 未归一化目标：为何直接采样困难；MCMC 用比值避开归一化常数
- 随机游走 MH（对称提议）与独立 MH（非对称修正）
- 二元正态 Gibbs 条件分布轮转；共轭正态后验闭式
- burn-in / thin / 积分自相关时间 IAT；Gelman-Rubin 多链 R̂
- HMC（路线图注记项）：leapfrog 最小实现，d=16 下与 RWM 的 IAT 对照

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | MH 2D 高斯矩 | 均值误差 < 3SE；协方差元素 rel < 10% |
| E2 | 强相关目标 IAT | Gibbs IAT / RWM-MH IAT < 1/3（ρ=0.95） |
| E3 | Gibbs 共轭后验 | 样本矩 vs 解析后验；同题 IAT 对照 |
| E4 | R̂ 诊断 | 4 链 burn-in 后 R̂ < 1.1 且持续 |
| E5 | thin 对照 | thin↑ 使存盘样本 lag-1 ACF / IAT 明显下降 |
| E6 | HMC（注记） | d=16 对角高斯：HMC IAT < 0.5×RWM IAT；接受率 ∈(0.6,1] |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | mcmc（MH / Gibbs / IAT / R̂ / 共轭后验） |
| `test/` | harness + test_main |
| `vendor/` | A2(dist/stats/gof) + C1(rng/mc) |

测试 suite：mh、gibbs、hmc、diagnostics

```bat
mingw32-make check-C2
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/C2-mcmc check TEST_ARGS=<suite|suite/case>
```
