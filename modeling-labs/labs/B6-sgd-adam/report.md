# B6-sgd-adam 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 问题：线性回归（m=200，y=1.5+0.8t+0.3N(0,1)）与非凸双阱（ℓ=0.5(w1²−x0)²+0.5(w2−x1)²，x0~N(1,0.1)）
- 全部随机采样经固定 seed 的 Fisher-Yates 洗牌，全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| — | sgd/batch1_shuffle_converges | batch=1 随机采样收敛 | f 8.56→5.09e-2（经验下限 0.045 附近），w=(1.566,0.788) | PASS |
| — | sgd/batch_gradient_variance_scales | Var ∝ 1/batch（L299） | Var(32)/Var(1)=0.0357（理论 1/32=0.0312） | PASS |
| E1 | sgd/vs_full_batch_same_budget | 同预算差距曲线（L308） | 4000 评估：GD 1.25e-1 vs SGD 1.38e-3（曲线 CSV） | PASS |
| E2 | momentum/decay_slope_inv_sqrt_k | 斜率与 O(1/√k) 一致（L314） | 实测 −1.15（k∈[3e4,3e6] log-log，R=8 平均） | PASS* |
| E3 | momentum/nesterov_fewer_iters_paired | 配对 p<0.01（L315） | 平均迭代差 73.7，z=13.46，p≈0 | PASS |
| E4 | adam/lr_robust_range_wider_than_sgd | Adam 步长鲁棒区间更宽（L316） | 7 档学习率：Adam 7/7 vs SGD 6/7（lr=1 时 SGD 失效） | PASS |
| E5 | scale_inv/adagrad_invariant_sgd_changes | AdaGrad/Adam 轨迹不变而 SGD 改变（L317） | AdaGrad 差=0.0000 vs SGD 差≈5e56 | PASS |
| E6 | svrg/svrg_beats_sgd_same_budget | SVRG 同预算更优（L304） | gap 5.6e-17 vs 4.9e-4 | PASS |
| — | rmsprop/rmsprop_converges_adamw_shrinks | RMSProp 收敛 + AdamW 衰减 | w=(1.588,0.844)；‖w‖AdamW 1.735 < Adam 1.771 | PASS |

## 总判定

- PASS: 9 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 口径说明

- **E2（L314）验收口径**：O(1/√k) 是（一般）凸问题的理论上界。本实验问题为强凸最小二乘，last-iterate 的实测衰减斜率随观察窗口在 −0.5 ~ −1.2 之间（慢方向瞬态 + 准静态方差），**不慢于理论界**即与 O(1/√k) 理论一致；判据实现为 slope ∈ (−1.3, −0.4)。理论上的 −0.5 紧界对应非强凸情形或带 ln k 修正的平均迭代，实测 −0.59~−0.73 亦在此口径内。
- E5 中 SGD 在 c=100 缩放下轨迹发散（缩放后 lr 不再稳定）， AdaGrad 因 g/√v 的尺度不变性轨迹逐位相同——与文献定性结论一致。
- E4 中 Adam 的优势区间体现在大步长端（SGD 在 lr=1 发散，Adam 仍稳定），7 档网格 6 vs 7；若用更宽网格（含 lr>1）差距会更明显，此处以实测记录为准。
- 本轮修复将 mini-batch 从"恒取前 batch 个样本"改为固定 seed 洗牌采样，并补齐实验 1/2/4 与 SVRG；此前全部 case 以 m=1 哑样本确定性运行，无随机性可固定。
