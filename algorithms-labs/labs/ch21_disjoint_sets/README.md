# 第 21 章：不相交集合（Disjoint Sets）

对应《算法导论》第三版第 21 章。实现按秩合并 + 路径压缩的并查集。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| MAKE-SET / FIND-SET / UNION | 21.3 | `disjoint_set.c` |
| 无向图连通分量 | 21.1 | `disjoint_set.c` |

## 实现说明

- 不相交集合森林：`parent` + `rank`；`ds_find` 路径压缩，`ds_union` 按秩合并。
- 连通分量：对每条边 `UNION`，再对根重新编号得到 `comp[]`。
- 摊还代价近似反 Ackermann 函数 α(n)（书中定理 21.4）。

## 构建与测试

```powershell
mingw32-make ch21
mingw32-make test-ch21
.\build\ch21_disjoint_sets\demo_disjoint_set.exe
```

## 阅读建议

1. 链表实现 vs 森林实现：UNION 的代价差异。
2. 路径压缩与按秩合并同时使用为何摊还近似常数。
3. MST（Kruskal）中已隐含并查集，可对照 `ch23_mst`。
