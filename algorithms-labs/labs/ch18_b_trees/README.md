# 第 18 章：B 树（B-Trees）

对应《算法导论》第三版第 18 章。实现最小度数 `t` 的 B 树。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| B-TREE-SEARCH | 18.2 | `btree.c` |
| B-TREE-INSERT / SPLIT-CHILD | 18.3 | `btree.c` |
| B-TREE-DELETE（借键/合并） | 18.3 | `btree.c` |

## 实现说明

- 每个结点关键码数在 `[t-1, 2t-1]`（根除外）；叶子同层。
- 插入前若根满则先分裂；删除后若根空且有孩子则收缩根。
- 重复键插入忽略；`t >= 2`。

## 构建与测试

```powershell
mingw32-make ch18
mingw32-make test-ch18
.\build\ch18_b_trees\demo_btree.exe
```

## 阅读建议

1. 为何 B 树高度 O(log_t n)，适合外存？
2. 分裂与合并如何维持最小/最大分支数。
3. 对照数据库/文件系统索引中的 B+ 树。
