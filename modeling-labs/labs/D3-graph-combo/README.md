# D3 图与组合优化

离散建模侧：最短路、生成树、拓扑排序、遍历（BFS/DFS）、最大流、TSP 谱系（NN/2-opt/Held-Karp）与 DP/指派精确基准。

## 知识点

- 图表示；BFS（层序性质）/DFS（深入优先）；拓扑排序
- 最短路：Dijkstra（负权拒算）、Floyd（互验）、Bellman-Ford（负权/负环，相对容差）
- MST：Prim / Kruskal
- 最大流：Edmonds-Karp（BFS 增广）；最大流-最小割对偶
- DP：背包 DP、指派枚举（多项式精确解 = 启发式对账基准；与 B4 分支定界联动）
- TSP：最近邻贪心、2-opt 局部搜索（改进分布 + 盆地结构）、Held-Karp O(n²2ⁿ)

## 判据

| ID | 实验 | 判据 |
|---|---|---|
| E1 | 最短路 | Dijkstra vs Floyd **100/100** 一致 |
| E2 | Held-Karp 基准 | gap 可测量可排序，排序跨 ≥10 实例稳定 |
| E3 | 2-opt | 显著优于最近邻（配对 **p&lt;0.01**） |
| E4 | 负权 | Bellman-Ford 负权解正确 + 负环检出；Dijkstra 负权拒算 |
| E5 | MST | Prim 与 Kruskal 权一致 |
| E6 | 遍历 | BFS 层序=单位权跳数分组；DFS 深入优先；BFS/DFS 同分量 |
| E7 | 实验 2（:653） | 随机起点改进分布 + 局部最优盆地统计（CSV 可复算） |
| E8 | 最大流（:647） | CLRS 网络=23 + 守恒/容量；随机图 maxflow==mincut |
| E9 | 指派/背包（:649） | D3 精确解 vs B4 vendor 分支定界逐实例一致 |

路线图原文锚点：

- 两最短路算法 100/100 实例结果一致
- Held-Karp 基准下各启发式相对 gap 可测量、可排序且排序跨实例稳定（≥ 10 实例）
- 2-opt 解质量显著优于最近邻贪心（配对检验 p < 0.01）
- 2-opt 局部搜索：随机初始回路的改进幅度统计；局部最优的盆地分布
- 最大流 Ford-Fulkerson（选做）；指派/背包/流的整数规划建模（与 B4 分支定界联动）

## 目录与测试

| 目录 | 内容 |
|---|---|
| `src/` | graph、tsp、dp（背包 DP + 指派枚举） |
| `test/` | harness + test_main |
| `vendor/` | A1 + A2 + B4(knapsack/simplex/assign) + C1(rng) |

测试 suite：shortpath、bfs、dfs、maxflow、assign、mst、graph、tsp、dp

```bat
mingw32-make check-D3
bin\test.exe --list
bin\test.exe
bin\test.exe tsp
bin\test.exe tsp/two_opt_better_than_nearest_neighbor
bin\test.exe maxflow assign dp bfs dfs
mingw32-make -C labs/D3-graph-combo check TEST_ARGS=<suite|suite/case>
```
