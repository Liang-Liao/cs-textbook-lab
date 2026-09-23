# D3-graph-combo 实验报告

## 环境

- 编译: gcc -std=c11 -O2 (MSYS2 UCRT64)
- 库: mlab 0.1.0 — graph（最短路/MST/拓扑/遍历/最大流）、tsp（NN/2-opt/Held-Karp）、dp（背包 DP/指派枚举）
- vendor: A1(vec/mat/linalg) + A2(dist/stats/gof) + B4(knapsack/simplex/assign) + C1(rng)
- seed: 最短路 81k 系列；MST 82k；TSP HK 83001；2-opt vs NN 83002；n16 83003；盆地 83004；背包 86001；指派 86002；最大流 86003

## 实验与判据

| 判据 ID | 条件（路线图） | 实测 | 结果 |
|---|---|---|---|
| E1-shortpath | Dijkstra vs Floyd **100/100** 一致 | **100/100**（fails=0） | PASS |
| E2-hk-gap | Held-Karp 基准 gap 可测量、可排序，排序跨 ≥10 实例稳定 | n=12×**12** 实例，rand≥nn≥2opt **12/12**；mean gap nn=1.82% 2opt≈0% rand=61.9% | PASS |
| E3-2opt | 2-opt 显著优于最近邻（配对 p&lt;0.01） | n=14×40：NN=3.475 2opt=3.390，p=**1.61e-5** | PASS |
| E4-bf | Bellman-Ford 负权与负环 | dist=(0,1,−1,2)；neg_cycle=**1** | PASS |
| E5-mst | Prim vs Kruskal | 30 图 mismatches=**0** | PASS |
| E6-bfs-dfs | BFS/DFS 行为正确（修复轮补零覆盖） | 已知图 BFS 序=层序、DFS 深入优先序；20 随机图层序↔单位权 Dijkstra 相容、BFS/DFS 同连通分量 | PASS |
| E7-basins | 实验 2：随机初始回路改进分布 + 盆地分布（:653） | n=12×5×150：mean improve=**51.3%**（750/750 全改进）；盆地/实例 ∈[2,5] ≪ 150，最大盆地占比 ≥**49%** | PASS |
| E8-maxflow | 最大流（:647 选做）+ 最小割对偶 | CLRS 网络 max-flow=**23.0**（流守恒+容量成立）；随机图 maxflow==mincut **10/10** | PASS |
| E9-assign-b4 | 指派与 B4 分支定界联动（:649） | D3 枚举 vs B4-BnB **30/30** 最优一致（含并列） | PASS |
| E10-knapsack-b4 | 背包 DP 真调 B4 vendor 对账 | DP vs B4-BnB vs B4-brute **30/30** 三方一致 | PASS |

补充：DAG 拓扑序正确；Dijkstra 对负权图显式拒算（rc=−2），BF 同图正确给出 (0,5,3)；n=16 HK 可算且 nn+2opt 达到 HK（gap=0）。

## 关键数据

- 最短路互验: `results/shortpath_agree.csv`
- MST 对账: `results/mst_agree.csv`
- HK gap 表: `results/tsp_hk_gaps.csv`
- 2-opt vs NN: `results/tsp_2opt_vs_nn.csv`
- 随机起点改进/盆地分布: `results/tsp_2opt_basins.csv`
- 最大流/最小割: `results/maxflow_mincut.csv`
- 背包三方对账: `results/knapsack_dp_b4_agree.csv`
- 指派对账: `results/assign_d3_b4_agree.csv`

## 结论与备注

- **最短路双算法互验**：树骨架 + 随机边连通无向图族上，单源 Dijkstra 全源拼接与 Floyd-Warshall 100/100 一致（容差 1e-9 相对）。Dijkstra 现对负权边显式返回 −2（此前静默给出错误结果）；Bellman-Ford 负环判定的容差相对化（1e-9·max(1,|dist|)），避免大量级距离下的误报。
- **BFS/DFS（修复轮补齐）**：此前 graph.c 遍历 API 零测试覆盖。现验证：已知图上 BFS 序=层序（且层序与单位权 Dijkstra 距离相容）、DFS 深入优先序确定；20 个随机图上 BFS 层分组性质成立、BFS/DFS 访问同一连通分量且序列均为合法排列。
- **Held-Karp 基准**：n=12 欧氏 TSP 上 gap 排序稳定为 随机 ≥ NN ≥ 2-opt，2-opt 多数实例达到 HK。该基准可直接给 C3/C5/C7 启发式对账。
- **随机起点 2-opt（实验 2 补齐）**：750 个随机初始回路全部被 2-opt 改进（均值 51.3%）；局部最优按规范形（旋转到 0 号城市 + 方向归一）聚类，每实例仅 2–5 个不同盆地（≪150 起点），最大盆地占比 49%–80%+ ——「局部最优呈盆地结构、随机起点分布不均」可直接从 CSV 复算。
- **最大流（选做落地）**：Edmonds-Karp（BFS 最短增广路）在 CLRS 经典网络得到已知最优 23，流量满足守恒与容量约束；随机有向图上与暴力最小割（2^(n-2) 子集枚举）逐实例相等，验证最大流-最小割对偶。
- **DP/枚举与 B4 联动**：背包 DP 从测试内联移入 `src/dp.c`（整数重量守卫 + 选择回溯），与 B4 vendor `mlab_knapsack_bb/brute` 30 实例三方一致；指派问题 D3 暴力枚举与 B4 `mlab_assign_bnb` 30 实例一致（整数代价含并列）。
- 注记：Or-opt 邻域在 C3 已有，本 lab 2-opt 路径独立实现；最小费用流仍未实现（路线图概览级）。
- common 增量: `mlab_graph_*`（含 `mlab_graph_maxflow_ek`）、`mlab_tsp_*`、`mlab_knapsack_dp`、`mlab_assign_brute`。

## 总判定

- PASS: 17 / FAIL: 0 — **PASS**
