# 第 19 章：斐波那契堆（Fibonacci Heaps）

对应《算法导论》第三版第 19 章。实现最小斐波那契堆的基本操作。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| FIB-HEAP-INSERT | 19.1 | `fib_heap.c` |
| FIB-HEAP-EXTRACT-MIN / CONSOLIDATE | 19.3 | `fib_heap.c` |
| FIB-HEAP-DECREASE-KEY / CUT | 19.4 | `fib_heap.c` |
| FIB-HEAP-UNION | 19.1 | `fib_heap.c` |

## 实现说明

- 根链表与孩子链表均为环形双向链表；`extract_min` 后 `consolidate` 按度合并。
- `decrease_key` 支持 cut + 级联 cut（简化实现，足够通过单元测试）。
- 摊还：`insert` O(1)，`extract_min` O(lg n)，`decrease_key` O(1) 摊还。

## 构建与测试

```powershell
mingw32-make ch19
mingw32-make test-ch19
.\build\ch19_fibonacci_heaps\demo_fib_heap.exe
```

## 阅读建议

1. 斐波那契堆为何适合 Dijkstra 中大量 `DECREASE-KEY`。
2. 势能函数 Φ = t + 2m 与三种操作的摊还分析。
3. 与二叉堆 / 配对堆的工程取舍。
