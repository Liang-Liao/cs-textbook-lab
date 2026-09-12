# 第 17 章：数据结构的扩张（Augmenting Data Structures）

对应《算法导论》第三版数据结构扩张一章（英文版 Ch.14）。

## 本章内容

| 结构/算法 | 书中节号 | 源文件 |
|-----------|----------|--------|
| 顺序统计树 OS-SELECT / OS-RANK | 14.2 | `order_stat_tree.c` |
| 区间树 INTERVAL-SEARCH | 14.3 | `interval_tree.c` |

## 实现说明

- 顺序统计树：**红黑树 + `size` 域**，插入/删除/查询 O(lg n)；`ost_validate` 校验红黑与 size。
- 区间树：按 `low` 为键的 BST，子树维护 `max_high`；查询与 `[low,high]` 相交的区间。
- OS-SELECT / OS-RANK 使用 **0-based** 秩（书中 1-based 需减 1）。

## 构建与测试

```powershell
mingw32-make ch17
mingw32-make test-ch17
.\build\ch17_augmentation\demo_augmentation.exe
```

## 阅读建议

1. 扩张步骤：选择扩充信息 → 写维护更新 → 用在查询/修改中。
2. 为何区间查询能用 `max_high ≥ low` 剪枝？
3. 红黑树旋转时如何同步维护 `size`（本实现旋转后 `set_size`）。
