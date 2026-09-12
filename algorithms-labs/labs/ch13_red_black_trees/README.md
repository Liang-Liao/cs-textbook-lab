# 第 13 章：红黑树（Red-Black Trees）

对应《算法导论》第三版第 13 章。实现带哨兵 `nil` 的红黑树旋转、插入与删除。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| LEFT-ROTATE / RIGHT-ROTATE | 13.2 | `red_black_tree.c` |
| RB-INSERT / RB-INSERT-FIXUP | 13.3 | `red_black_tree.c` |
| RB-DELETE / RB-DELETE-FIXUP | 13.4 | `red_black_tree.c` |
| 红黑性质校验 | 13.1 | `rb_validate` |

## 实现说明

- 与书中一致使用共享哨兵 `T.nil`（黑色）。
- 重复键插入返回 `NULL`。
- `rb_validate` 检查：根为黑、无红红相连、各路径黑高相等。

## 构建与测试

```powershell
mingw32-make ch13
mingw32-make test-ch13
.\build\ch13_red_black_trees\demo_rbtree.exe
```

## 阅读建议

1. 五种情况画图：插入 fixup 的 uncle 红/黑，以及三种旋转。
2. 删除是本章最难部分——对照书中图 13.6/13.7。
3. 红黑树高度上界 2 lg(n+1) 保证了 O(lg n) 动态集合操作。
