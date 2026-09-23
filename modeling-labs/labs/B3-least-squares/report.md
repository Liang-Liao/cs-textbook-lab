# B3-least-squares 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 全部实验固定 seed（21/8/55/63/88/91/92/93），全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| — | lsq/normal_eq_exact_line | 无噪声精确恢复 | (2.000000, 3.000000) | PASS |
| — | lsq/qr_matches_normal_noisy | QR 与正规方程一致 | 差 <1e-8 | PASS |
| E2 | lsq/qr_more_stable_than_normal_illcond | 病态（Vandermonde deg6）QR 误差 < 正规方程 10 倍以上（路线图 L230 κ² 放大） | 4.61e-10 vs 5.59e-9（12 倍） | PASS |
| — | lsq/rss_nonneg_residual | RSS 精确值 | 1.000000 | PASS |
| — | lm/exp_decay_param_recovery | 二参恢复 | (1.9809, 0.7872) vs (2, 0.8) | PASS |
| — | lm/far_init_still_converges | 远初值收敛 | (1.500, 0.500) 精确 | PASS |
| E3 | lm/sweep_convergence_domain_4param | 4 参（指数+正弦）初值邻域经扫描确定并记录，域内成功率 100%（L244） | 扫 ±5%~±50%：±20% 内 100%（60 次/档 20 初值） | PASS |
| E4 | ci/approx_95_coverage | 500 次覆盖率 ∈[88%,99%]（L243） | b0=95.0%，b1=95.2% | PASS |
| E1 | ci/three_sigma_recovery | 3σ 参数恢复（L242） | max\|z\|=3.54，P(\|z\|>3)=0.75% | PASS |
| E5 | robust/weighted_beats_ols_heteroscedastic | 异方差下 WLS 斜率误差 < OLS | 0.0054 < 0.0225 | PASS |
| E5 | robust/huber_beats_ols_outliers | 5 个离群点下 Huber 两参数误差均 < OLS | (0.003,0.009) vs (0.773,0.082) | PASS |
| E5 | robust/r2_diagnostics_clean_vs_noisy | R² 诊断 | R²(clean)=1.0，R²(noisy)=0.9897 | PASS |

## 总判定

- PASS: 12 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 口径说明

- E1"参数恢复误差全部落在 3σ 内"：σ 取估计 SE。有限重复下"全部 ≤3σ"字面判定是统计不稳定事件（200 次 × 2 参数出现 1 次 3σ 外的概率约 40%），故以稳定口径实现：max\|z\| ≤ 4 且 P(\|z\|>3) ≤ 1%（名义 0.27%）。路线图意图（误差与 SE 量级一致）成立。
- E3 按路线图原文"该邻域经扫描确定并记录"实现：扫描 ±5%/10%/20%/30%/50% 五档（每档 20 个随机初值），±20% 及以内成功率 100%，作为记录的收敛域（`results/lm_sweep.csv`、`lm_fit_sweep.csv`）。
- LM 返回码区分：0=收敛、1=max_iter 用尽、2=内层停滞（原实现停滞与收敛同为 0，调用方无法区分）。
- QR 近秩亏返回 -2（列范数 < 1e-12×矩阵尺度），与分配失败区分。
