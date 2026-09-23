# B1-line-search-gd 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 全部实验确定性（谱构造 seed=5），全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| E1 | linesearch/golden_parabola_minimum | 抛物线最小点 | x*=1.40000000 | PASS |
| E1 | linesearch/golden_zero_minimum | 夹逼到 0 | x*=-1.45e-13，fmin=1.0e-26 | PASS |
| E1 | linesearch/golden_shrink_ratio_0618 | 实测收缩比与 0.618 相对误差 <1%（路线图 L188 原文） | 0.61803399，rel=1.53e-10 | PASS |
| E2 | linesearch/armijo_backtrack_quad | 接受下降步 | α=0.25，fe=4 | PASS |
| E2 | linesearch/armijo_stricter_c1_not_fewer_evals | 更严 c1 试探不更少 | c1=0.4：fe=5 ≥ 4 | PASS |
| E2 | linesearch/armijo_rejects_ascent_direction | 非下降方向被拒绝 | α=0（此前会被平凡接受，已修复） | PASS |
| E4 | wolfe/wolfe_strong_accepts_half_exact | 0.5αe 直接满足两条件 | α=0.071478=0.5αe，fe=2 | PASS |
| E4 | wolfe/wolfe_strong_zoom_from_overshoot | α0=3αe 触发 zoom 后两条件成立 | α=0.214433∈[0.1,1.9]αe，f 下降 | PASS |
| E4 | wolfe/wolfe_rejects_ascent_direction | 上升方向拒绝 | α=0 | PASS |
| E3 | steepest_descent/well_conditioned_converges | 良态收敛 | κ=2：23 iters | PASS |
| E3 | steepest_descent/ill_conditioned_more_iters | 迭代比与 κ 同数量级（路线图 L189 原文） | 2122/58=36.6（κ 比 40） | PASS |

## 总判定

- PASS: 11 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 结论与备注

- 修复项：①黄金分割收缩比判据本轮才真正落地（此前测试无该测量、report 引用遗留 CSV 声称 PASS）；②Armijo 对非下降方向返回 0（此前 `ft <= f0 + c1·α·slope` 在 slope≥0 时可被平凡满足）；③新增强 Wolfe（bracket+zoom）；④API 变更：`mlab_golden_section` 增加可选 `widths_out`，`mlab_armijo_backtrack` 失败语义明确为返回 0。下游 vendor（B2/B5/C9/M1/M2/M3）随后续各 lab 修复批次同步。
- `gd_traj.csv` 恢复落盘：κ=5/200 两条完整残差轨迹，锯齿与病态放大可见；遗留 `gd_iters.csv` 由测试输出替代删除。
