# B4-lp-ip 实验报告

## 环境

- Windows 10 / MSYS2 UCRT64，gcc -std=c11 -O2，仅链接 libm
- 结构 src + test + vendor；测试入口 `bin/test.exe`（全量/`--list`/suite/suite-case）
- 随机实验固定 seed（LP 实例 100、背包 101、指派 202），全量运行自动重算 `results/*.csv`

## 判据与实测

| ID | case | 判据 | 实测 | 结论 |
|---|---|---|---|---|
| — | simplex/max_sum_known_optimum | 已知 LP 最优 | obj=-2.000000 | PASS |
| — | simplex/solution_components_nonnegative | 解非负 | neg=0 | PASS |
| E1 | simplex/dual_gap_and_complementary_slackness | 原始-对偶间隙=0、互补松弛（路线图 L265） | y=-1.000000，gap=0，comp=0 | PASS |
| E1b | simplex/matches_vertex_enumeration | 单纯形 vs 顶点枚举互验（L260） | 30/30 实例一致 | PASS |
| — | knapsack/tiny_brute_known | 小实例已知解 | 7.0 | PASS |
| — | knapsack/bb_matches_brute_n12 | n=12 对账 | 40.1767 = 40.1767 | PASS |
| — | knapsack/zero_capacity_value_zero | 零容量边界 | 0.0 | PASS |
| E2 | knapsack/bnb_vs_brute_100_instances | 100/100 实例一致（L262/266） | 100/100 | PASS |
| E2b | assign/bnb_matches_enum_n6 | 指派 BnB vs 全排列（L262） | 50/50 一致（n=6） | PASS |
| — | assign/known_optimum_n3 | 指派已知最优 | 5.0，perm=(1,0,2) | PASS |
| E3 | cuts/gomory_demo_known_ilp | Gomory 割闭环整数最优并与暴力对账（L257） | 3 割 → (3,1)/11.0000，brute=11 | PASS |
| — | cuts/integer_vertex_no_cut_needed | 整数顶点无需割 | obj=26，cuts=0 | PASS |

## 总判定

- PASS: 12 / FAIL: 0 — **PASS**（`bin/test.exe` 退出码 0）

## 修复与新增说明

- **修复（P1）**：① Phase II 进基候选原先包含人工变量列，人工变量可重新进基导致误判不可行或违反 Ax=b——现限制 `j<n`；② 对偶解 `y_out` 符号翻转（原返回 −y）——现按 `y_i = −cost_row[n+i]` 还原，并由"间隙=0 + 互补松弛"case 锁定（此前对偶输出零测试）；③ b<0 行预处理（乘 −1）；④ 死代码清理；⑤ `mlab_knapsack_brute` 的 `1UL<<n` 改 `1ULL` 并加 n≤62 守卫。
- **新增**：指派问题模块（`assign.c`：BnB 行最小下界 + 全排列枚举对账）；Gomory 割平面最小演示（`cuts.c`：顶点枚举 LP 松弛 + 紧行基的纯整数舍入割 + s→x 转换 + 整数缩放；演示实例 3 割收敛并与暴力枚举对账）。演示为"概览"级可运行落点，不处理退化/无界等一般情形。
