# C1 随机数生成与蒙特卡洛积分

多发生器 RNG、逆变换/拒绝采样、MC 与重要性采样（含权重诊断）、Sobol 低差异序列。

## 知识点

- splitmix64 / xorshift64* / LCG / MT19937（MT 扭转递推与标准一致；播种用 splitmix 填充，见 `src/rng.c` 注记）
- 质量测试：卡方均匀性、滞后自相关（|ρ| < 2/√N）、间隔检验（截尾几何 GOF）
- 逆变换（连续 + 离散分布）、拒绝采样接受率 1/c
- 正态采样：Box-Muller、Marsaglia 极法（矩 + KS 双口径）
- MC 收敛斜率 −0.5 与 IS 方差；IS 权重 ESS / 坏比值诊断
- Sobol 低差异序列（注记项）：低维方向数，同 N 误差 vs 伪随机对照

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 发生器质量 | 卡方拒绝率 ∈[0.03,0.07]；acf1 与超界比例满足 \|ρ\|<2/√N 口径；Box-Muller/Marsaglia 矩+KS |
| E2 | 逆变换 KS | 指数/正态拒绝率 ∈[0.03,0.07]（1000 次）；离散逆变换卡方同区间 |
| E3 | 拒绝采样 | 接受率 vs 理论 rel<2% |
| E4 | MC 斜率 | π 与高斯积分斜率 −0.5±0.05；IS 方差更小 |
| E5 | IS 权重诊断 | 好提议 ESS/n>0.90；坏提议 ESS 崩塌且坏比值报警 |
| E6 | Sobol（注记） | 前 16 点与 scipy 参考一致；同 N 误差 < 0.5×MC |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | rng / mc（含 gap test、离散逆变换、Sobol、IS 权重诊断） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A2(dist/stats/gof) |

测试 suite：rng、inverse_transform、rejection、mc、importance_sampling、qmc

```bat
mingw32-make check-C1
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/C1-rng-mc check TEST_ARGS=<suite|suite/case>
```
