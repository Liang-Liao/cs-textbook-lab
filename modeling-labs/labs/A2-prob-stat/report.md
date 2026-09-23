# A2-prob-stat 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- RNG：splitmix64（默认）；全部实验固定 seed，全量运行自动重算 `results/*.csv`
- seed：KS=11（三种分布共用流）、CLT 矩=20260214、CLT 卡方=20260215、卡方均匀=5、CV=99、对偶=301/302、CRN=1000+r/5000+r、分层=303、MLE/CI=401/402/403

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E1 | clt/exp_means_match_clt | 样本均值矩贴近理论 | E=1.0030（理论 1），sd=0.1858（理论 0.1826） | PASS |
| E1 | clt/chi2_normal_fit_standardized | 直方图与正态对比卡方检验不拒绝 | n=100、20 等概率箱：chi2=30.55，df=17，p=0.0226>0.01 | PASS |
| E2 | gof/ks_type1_uniform | 1000 次重复拒绝率 ∈[0.03,0.07]（路线图 L138 原文） | unif=0.053，exp=0.053，norm=0.044 | PASS |
| — | gof/chi2_uniform_large_n | 真均匀不被拒绝 | chi2=5.28，p=0.809 | PASS |
| E3 | control_variate/exp_u_reduction_vs_theory | 缩减比与理论相对误差 <10%（路线图 L139 原文） | 0.0168 vs 0.0163（3.1%） | PASS |
| E3 | control_variate/optimal_c_beats_plain_and_2c | 最优 c* 方差显著低于 plain，且不劣于 2c* | var(c*)/var(plain)=0.0168，var(2c*)/var(plain)=1.015 | PASS |
| E4 | variance_reduction/antithetic | 对偶变量显著降方差 | VR=0.0308（等预算理论 0.0323） | PASS |
| E4 | variance_reduction/crn | 公共随机数差值方差远小于独立 | 0.0033（理论≈0.003） | PASS |
| E4 | variance_reduction/stratified | 分层采样显著降方差 | 0.0105（m=10 理论≈0.015） | PASS |
| E5 | estimation/mle_normal_moments | 正态 MLE 矩恢复 | mu=0.0024，sigma=0.9911（1/n 口径） | PASS |
| E5 | estimation/ci_mean_coverage_95 | z-CI 覆盖率≈0.95 | 500 次：0.956 | PASS |
| E5 | estimation/ci_width_scales_1_over_sqrt_n | 宽度比 = √(25/100)=0.5 | 0.5000 | PASS |
| — | rng 一组（polar 矩/逆变换 KS/Poisson/Bernoulli/卡方 API/ACF 界） | 各采样器矩与质量界 | 全部通过（D=0.0138<0.0304 等） | PASS |
| — | rng/kind_name_labels | 四种发生器名称标签 | splitmix64/xorshift64*/lcg-mmix/mt19937 + unknown | PASS |
| — | rng/seed_kind_and_u64_all_generators | seed_kind 写 kind；同 seed 可复现；异发生器可区分；u64/uniform 可用 | 四 kind 均 repro 且序列互异；非法 kind 回退 splitmix64 | PASS |
| — | rng/autocorr_direct_analytic | mlab_autocorr 对账解析序列 | rho(1)=0.4，rho(2)=-0.1；lag0/不足返回 0 | PASS |
| — | dist/erf_landmarks | erf(1)=0.8427008 | 0.842700793 | PASS |

## 总判定

- PASS: 30 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 备注

- KS 检验统一调用 gof 正本 `mlab_ks_statistic`（此前测试内联重算、正本零覆盖，已修正）。
- CLT 卡方实验取 n=100（n=30 时指数均值的偏度仍会拒绝正态拟合——旧遗留 CSV 中 p=2.3e-15 的矛盾由本实验口径澄清）。
- 旧 `control_variate.csv` 由 `variance_reduction.csv`（四种方法）替代。
- 计划批 1 A2④ 补测：`rng/kind_name_labels`、`rng/seed_kind_and_u64_all_generators`、`rng/autocorr_direct_analytic`，以及 `control_variate` 套件第 2 case `optimal_c_beats_plain_and_2c`。
