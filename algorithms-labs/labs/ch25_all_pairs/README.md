# 第 25 章：全源最短路径（All-Pairs Shortest Paths）

对应《算法导论》第三版第 25 章。实现 Floyd-Warshall 与 Johnson 算法。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| FLOYD-WARSHALL | 25.2 | `all_pairs.c` | Θ(V³) |
| JOHNSON（重赋权 + 每源 Dijkstra） | 25.3 | `all_pairs.c` | O(V² lg V + VE)（本实现稠密 O(V³)） |

## 实现说明

- 矩阵存储；`APSP_INF` 表示无边。
- Floyd-Warshall：负环检测（对角线为负）。
- Johnson：虚拟源点 Bellman-Ford 重赋权后运行稠密 Dijkstra。
- 书中图 25.1 的距离矩阵在测试中逐项核对。

## 构建与测试

```powershell
mingw32-make ch25
mingw32-make test-ch25
.\build\ch25_all_pairs\demo_all_pairs.exe
```

## 阅读建议

1. Floyd-Warshall 中间点 `k` 的含义；如何存前驱做路径重构。
2. Johnson 为何允许负边（无负环）时仍正确。
3. 与逐源 Bellman-Ford / Dijkstra 的适用场景对比。
