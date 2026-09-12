# 第 20 章：van Emde Boas 树

对应《算法导论》第三版第 20 章。实现带 min/max、cluster/summary 的 vEB 树。

## 本章算法

| 算法 | 书中节号 | 源文件 | 期望复杂度 |
|------|----------|--------|------------|
| MEMBER / INSERT / DELETE | 20.1–20.2 | `veb.c` | O(lg lg u) |
| SUCCESSOR / PREDECESSOR | 20.1–20.2 | `veb.c` | O(lg lg u) |

## 实现说明

- 全集 `U={0..u-1}`，`u` 为 2 的幂；空标记 `VEB_NIL = -1`。
- 递归实现：`high(x)=x/sqrt(u)`，`low(x)=x mod sqrt(u)`。
- 空 cluster 插入时直接写 min/max（书中技巧），避免再递归一层。

## 构建与测试

```powershell
mingw32-make ch20
mingw32-make test-ch20
.\build\ch20_van_emde_boas\demo_veb.exe
```

## 阅读建议

1. 递归式 `T(u)=T(lg u)+O(1)` 得到 `O(lg lg u)`。
2. 前驱/后继为何要结合 cluster 与 summary。
3. 与红黑树、跳表在不同 `u` 下的实用对比。
