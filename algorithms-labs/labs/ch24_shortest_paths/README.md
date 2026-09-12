# 第 24 章：单源最短路径（Single-Source Shortest Paths）

对应《算法导论》第三版第 24 章。实现 Bellman-Ford、DAG 最短路径与 Dijkstra。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| BELLMAN-FORD | 24.1 | `shortest_paths.c` | O(VE)，可检负环 |
| DAG-SHORTEST-PATHS | 24.2 | `shortest_paths.c` | O(V·E)，按邻接扫边表 |
| DIJKSTRA（数组版） | 24.3 | `shortest_paths.c` | O(V·E)，稠密图 O(V²) |

## 实现说明

- 边表有向图；`dist` 用 `long long`，不可达为 `LLONG_MAX`。
- Bellman-Ford：完整 V−1 轮松弛后若仍可松弛则有负环（返回 0）。
- Dijkstra：要求边权 ≥ 0，否则返回 0。
- 书中图 24.6 从 `s` 出发：`d = [0, 8, 9, 5, 7]`。

## 构建与测试

```powershell
mingw32-make ch24
mingw32-make test-ch24
.\build\ch24_shortest_paths\demo_shortest_paths.exe
```

## 阅读建议

1. 松弛操作（relaxation）与三角不等式。
2. 为何 DAG 上按拓扑序松弛一轮即可。
3. Dijkstra 正确性依赖非负权；有负权用 Bellman-Ford。
