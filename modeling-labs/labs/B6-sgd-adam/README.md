# B6 随机梯度与自适应优化

SGD/动量/Nesterov/AdaGrad/RMSProp/Adam/AdamW 与 SVRG，真实 mini-batch 随机采样（固定 seed 洗牌）。

## 知识点

- Robbins-Monro 随机逼近；mini-batch 随机采样（Fisher-Yates 洗牌）与梯度方差 ∝ 1/batch（L298-299）
- 步长调度：常数 / ∝1/k / ∝1/√k（L307-309）
- 动量与 Nesterov 加速；AdaGrad→RMSProp→Adam→AdamW 谱系（bias correction、解耦权重衰减）
- SVRG 方差缩减（L304 概览的可运行实现）
- scaled 不变性：损失纵向缩放 c 倍，AdaGrad/Adam 轨迹不变而 SGD 显著改变（L317）

## 判据（路线图 L313–317 原文）

| ID | 实验 | 判据（原文） | 实测 |
|---|---|---|---|
| E1 | SGD vs 全批 | 同预算次优性差距曲线落盘（L308） | 4000 评估：GD gap=1.25e-1、SGD=1.38e-3，曲线 CSV PASS |
| E2 | 递减步长 | 实测次优差距衰减斜率与 O(1/√k) 理论一致（L314，验收口径见 report） | 斜率 −1.15（不慢于理论界）PASS |
| E3 | Nesterov | Nesterov 版迭代数显著少于普通动量（p<0.01） | 20 实例配对：z=13.5，p≈0 PASS |
| E4 | Adam 鲁棒区间 | Adam 对步长的鲁棒区间显著宽于 SGD | 双阱非凸 7 档学习率：Adam 7/7 vs SGD 6/7 PASS |
| E5 | scaled 不变性 | c 倍缩放下 AdaGrad/Adam 轨迹几乎不变而 SGD 显著改变 | AdaGrad 差=0 vs SGD 差≈5e56 PASS |
| E6 | SVRG | 同预算下 SVRG 优于 SGD | gap 5.6e-17 vs 4.9e-4 PASS |

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | sgd（7 种更新规则 + 随机 epoch 驱动 + 调度 + SVRG） |
| `test/` | harness + test_main（多场景 suite/case） |
| `vendor/` | A1(vec) + A2(rng 及其编译依赖) |
| `results/` | 全量运行自动重算落盘（sgd_vs_full / decay_slope / momentum_paired / adam_grid / scale_inv / svrg） |

测试 suite：sgd、momentum、adam、scale_inv、svrg、rmsprop

```bat
mingw32-make check-B6
bin\test.exe --list
bin\test.exe
bin\test.exe <suite>
bin\test.exe <suite>/<case>
mingw32-make -C labs/B6-sgd-adam check TEST_ARGS=<suite|suite/case>
```
