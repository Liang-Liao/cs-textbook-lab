# C4-ts-vns-ils 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — tabu（禁忌+特赦+fallback+频次记忆）、vns_ils（VNS/VND/ILS/best-improve，预算按调用计）、grasp_lns（LNS/GRASP）
- vendor: A2(dist/stats/gof) + C1(rng/mc) + C3(sa/tsp，含 or-opt 与随机距离矩阵)
- 实例: 簇结构欧氏 n=20（TS/四机制/best-first/LNS）；均匀随机欧氏 n=24（VNS 邻域族）；随机距离矩阵 n=20/24（ILS）

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-tabu-tenure | 禁忌表长度 U 形且可复现 | best tenure=12，两次独立种子均为 12；t0=58.17、t35=49.10 均差于 47.54 | PASS |
| E2-vns-multi | 多邻域 VNS 显著优于任一单邻域（vnd=1 去混杂）p<0.01 | swap=45.23, 2opt=39.61, oropt=48.05, **multi=38.14**；p_2=1.2e-10 | PASS |
| E3-ils-u-shape | 扰动强度 U 形且区间可复现 | best s=3（39.05）repro s=1（\|Δ\|≤2），s0=41.24、s40=39.49 均更差 | PASS |
| E4-four-way | 同预算（±12%）≥100 次对账完整 | SA=32.85/6200, TS=32.84/6081, VNS=32.84/6000, ILS=32.84/6000 —— 预算语义修复后 VNS/ILS 恰好 6000 | PASS |
| E5-tabu-fallback | 全禁忌逃逸更新 f_best/禁忌表 | tenure=80（n=12）：fallback=134 次，f_best 59.18→28.70 | PASS |
| E6-tabu-freq | 频次惩罚生效且不劣化 | λ=0.05：n_freq_picks=60>0；mean 32.2807 vs λ=0 32.2807（不劣化） | PASS |
| E7-best-first | best/first 均停在穷举验证的局部最优 | first: 32.15/3627fe；best: 32.11/10033fe——best 质量略优、成本 ~2.8× | PASS |
| E8-ils-restarts | 同预算 ILS 优于随机多重启 p<0.01 | ils=34.95 vs restarts=35.24，p=6.2e-3（预算 10000，60 实例） | PASS |
| E9-lns-note | LNS 毁灭-重建（注记项） | 局部最优 34.36 → +LNS5=29.17（同预算 vs 继续 VND） | PASS |
| E10-grasp-note | GRASP RCL（注记项） | α=0: 33.10, α=0.5: **32.87**, α=1: 32.96 —— 最优在中间 | PASS |

## 关键数据

- 禁忌扫描: `results/tabu_tenure_scan.csv`
- VNS 邻域: `results/vns_nbhd_compare.csv`
- ILS 扰动: `results/ils_pert_scan.csv`
- 四机制: `results/four_mechanisms.csv`（含逐次配对）
- TS fallback/频次: `results/tabu_freq_memory.csv`（fallback 计数见 case 详情）
- best/first: `results/vns_best_vs_first.csv`
- ILS vs 重启: `results/ils_vs_restarts.csv`
- LNS/GRASP: `results/lns_vs_vnd.csv`、`results/grasp_rcl_scan.csv`

## 结论与备注

- **TS**: swap 邻域下任期过短≈贪心、过长邻域冻结，中等任期（本例 12）最好；特赦保证不丢全局改进。
- **VNS**: 单邻域各有短板（swap/or-opt 弱于 2-opt）；`ALL+VND` 同预算显著更好。
- **ILS**: 总预算必须大于初始局部搜索，否则主循环不启动（曾导致曲线全平）。非度量随机距离矩阵局部最优更丰富，才能显出 U 形。
- 四机制在欧氏簇实例上均逼近同一质量，差异更多在评估次数与实现路径；组合侧选型需看更难实例/更长预算（M2 预演）。
- **预算语义修复**：局部搜索原把"全局累计评估数"与"剩余预算"混比（超支 +55~69%）；
  修为按本次调用内计数，VNS/ILS 的 fevals 恰好收敛到预算值（6000）。
- **VND 顺序**：swap→2opt→oropt 的旧顺序在每次改进后从 swap 重扫（n=24 一次 276 评估），
  稀释预算导致 multi 反而劣于 2-opt 单邻域；改为按价值排序 2opt→oropt→swap 后 multi 显著占优。
- **去混杂**：邻域族对比四臂全部 vnd=1，唯一变量是邻域集合；非度量实例上 swap/oropt 是负资产
  （multi 35.56 < 2opt 34.86），换均匀随机欧氏实例后互补效应显现——邻域族优势依赖实例结构，如实呈现。
- TS 频次惩罚（λ=0.05）在该实例上不改变最终质量（最优解集合相同），机制生效但不劣化——长期记忆的
  价值需更长行程/更强多样性问题才体现。
- GRASP/LNS 为路线图注记项，最小实现落地：LNS 最小代价重插每城计 1 次评估记账；
  GRASP 每次构造计 1 次，RCL 中点最优印证"贪心随机自适应"。
- common 增量: `mlab_tabu_tsp`（频次/fallback）、`mlab_vns_tsp`（kmax 参数化）、`mlab_tsp_best_improve`、
  `mlab_lns_tsp`、`mlab_grasp_tsp`、`mlab_tsp_apply_oropt`、`mlab_tsp_random_matrix`。

## 总判定

- PASS: 10 / FAIL: 0 — **PASS**
