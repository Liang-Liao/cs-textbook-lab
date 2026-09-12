# 第 22 章：图的基本算法（Elementary Graph Algorithms）

对应《算法导论》第三版第 22 章。实现 BFS、DFS、拓扑排序与强连通分量。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| 邻接表建图 / 转置 | 22.1 | `graph.c` |
| BFS | 22.2 | `graph.c` |
| DFS（发现/结束时间） | 22.3 | `graph.c` |
| 拓扑排序（含环检测） | 22.4 | `graph.c` |
| Kosaraju 强连通分量 | 22.5 | `graph.c` |
| BFS 计时 | — | `bench_bfs.c` |

## 实现说明

- 有向图邻接表；BFS/DFS 用显式队列/栈，避免深递归。
- 拓扑排序 = DFS 结束时间的逆序；**检测回边**（灰点）判定有环并返回 0。
- Kosaraju：先在 G 上 DFS 记结束序，再在 G^T 上按逆结束序 DFS。

## 构建与测试

```powershell
mingw32-make ch22
mingw32-make test-ch22
.\build\ch22_elementary_graph\demo_graph.exe
```

## 阅读建议

1. BFS 为何能给出无权图最短路径？
2. DFS 边分类：树/回/横/前向边与环检测的关系。
3. 书中图 22.9 的四个 SCC 与转置第二次 DFS 的对应。
