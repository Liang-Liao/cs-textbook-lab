# 第 23 章：最小生成树（Minimum Spanning Trees）

对应《算法导论》第三版第 23 章。实现 Kruskal 与 Prim 算法。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| 并查集（按秩合并+路径压缩） | 21.3 | `mst.c` | 近似常数 |
| MST-KRUSKAL | 23.2 | `mst.c` | O(E lg E) |
| MST-PRIM（数组版） | 23.2 | `mst.c` | O(V·E)，稠密图 O(V²) |

## 实现说明

- 无向带权图，边表存储；Kruskal 按边权排序 + 并查集。
- Prim 使用数组最小键，但取邻接靠线性扫描边表，整体 O(V·E)（稠密图 O(V²)）；堆优化可到 O(E lg V)。测试在 V=9 上与 Kruskal 权值一致。
- 书中图 23.1 的 MST 总权为 **37**。

## 构建与测试

```powershell
mingw32-make ch23
mingw32-make test-ch23
.\build\ch23_mst\demo_mst.exe
```

## 阅读建议

1. 贪心安全边：切割性质（定理 23.1）。
2. Kruskal 与 Prim 对应不同切割策略；何时用堆优化 Prim 到 O(E lg V)。
3. 练习：23.2-4 连通分量上的最小生成森林。
