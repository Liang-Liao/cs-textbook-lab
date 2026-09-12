# 第 10 章：基本数据结构（Elementary Data Structures）

对应《算法导论》第三版第 10 章。实现栈、队列、双向链表与二叉树遍历。

## 本章内容

| 结构/算法 | 书中节号 | 源文件 |
|-----------|----------|--------|
| 栈（数组） | 10.1 | `stack.c` |
| 循环队列 | 10.1 | `queue.c` |
| 带哨兵双向链表 | 10.2 | `linked_list.c` |
| 二叉树前/中/后序 | 10.4 | `binary_tree.c` |

## 实现说明

- 栈：`top` 为元素个数，空为 0；上溢/下溢用 `CLRS_ASSERT`。
- 队列：循环数组；满判据为 `count == capacity-1`（与书中留空一格一致）。
- 链表：哨兵 `nil` 循环双向；`prepend`/`append`/`delete`/`search`。
- 树：`tree_height` 返回最长根到叶路径上的**结点数**（叶为 1）。

## 构建与测试

```powershell
mingw32-make ch10
mingw32-make test-ch10
.\build\ch10_elementary_ds\demo_elementary_ds.exe
```

## 阅读建议

1. 为什么队列要区分 full/empty（或用 count）？
2. 哨兵结点如何去掉双链表 delete 中的边界分支？
3. 三种遍历各自适合什么应用（表达式树、求值、释放）。
