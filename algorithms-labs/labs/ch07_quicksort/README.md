# 第 7 章：快速排序（Quicksort）

对应《算法导论》第三版第 7 章。实现 Lomuto 划分、随机化快排与 Hoare 划分。

## 本章算法

| 算法 | 书中节号 | 源文件 | 期望复杂度 |
|------|----------|--------|------------|
| PARTITION（Lomuto） | 7.1 | `quicksort.c` | Θ(n) 一次划分 |
| QUICKSORT | 7.2 | `quicksort.c` | 期望 Θ(n lg n) |
| RANDOMIZED-QUICKSORT | 7.3 | `quicksort.c` | 期望 Θ(n lg n) |
| HOARE-PARTITION | 练习 7-1 | `quicksort.c` | 与快排配套 |

## 实现说明

- 0-based：书中 `A[p..r]` 对应本实现同名下标；`partition` 返回枢轴最终下标 `q`，左右子问题为 `[p..q-1]`、`[q+1..r]`。
- Hoare 划分返回 `q` 后，左右为 `[p..q]`、`[q+1..r]`（与书中问题 7-1 一致）。
- 递归先处理较短的一侧，再循环处理较长一侧，降低栈深到 O(lg n)。
- 最坏情况（已序 + 固定选末元）仍是 Θ(n²)，可用随机化缓解。

## 构建与测试

```powershell
mingw32-make ch07
mingw32-make test-ch07
.\build\ch07_quicksort\demo_quicksort.exe
```

## 阅读建议

1. 对照书中图 7.1 手推 `partition` 的指针移动。
2. 比较 Lomuto 与 Hoare：交换次数、相等元素下的表现。
3. 练习：7.4-4 为何快排在小数组上可切换插入排序（可作扩展）。
