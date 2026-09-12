# 第 12 章：二叉搜索树（Binary Search Trees）

对应《算法导论》第三版第 12 章。实现带父指针的 BST 基本操作。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| TREE-SEARCH | 12.2 | `bst.c` |
| MINIMUM / MAXIMUM | 12.2 | `bst.c` |
| SUCCESSOR / PREDECESSOR | 12.2 | `bst.c` |
| TREE-INSERT | 12.3 | `bst.c` |
| TREE-DELETE / TRANSPLANT | 12.3 | `bst.c` |
| INORDER-WALK | 12.1 | `bst.c` |

## 实现说明

- 节点含 `parent`，与书中一致；`bst_delete` 按三种情形（无左/无右/两子）处理。
- 重复键插入返回 `NULL` 且不改树（可按需改为允许重复）。
- inorder 输出必为升序，测试中用随机插入 + 删除交叉验证。

## 构建与测试

```powershell
mingw32-make ch12
mingw32-make test-ch12
.\build\ch12_bst\demo_bst.exe
```

## 阅读建议

1. 画出书中 15,6,18… 插入后的树形，对照 delete 6 与 delete 15。
2. 为何最坏高度 Θ(n)？下一章红黑树如何保证 O(lg n)。
3. 练习：12.2-5 中序后继若无右子时只能沿父链上爬。
