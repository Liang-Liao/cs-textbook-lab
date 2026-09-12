# 第 6 章：堆排序（Heapsort）

对应《算法导论》第三版第 6 章。实现最大堆、堆排序与最大优先队列。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| MAX-HEAPIFY | 6.2 | `heapsort.c` | O(lg n) |
| BUILD-MAX-HEAP | 6.3 | `heapsort.c` | O(n) |
| HEAPSORT | 6.4 | `heapsort.c` | Θ(n lg n)，原地 |
| 最大优先队列 | 6.5 | `priority_queue.c` | insert/extract O(lg n) |

## 与伪码的差异

- 书中下标从 **1** 开始；本实现从 **0** 开始：  
  `parent(i)=(i-1)/2`，`left(i)=2i+1`，`right(i)=2i+2`。
- 书中 `MAX-HEAP-INSERT` 先插入 \(-\infty\) 再 `HEAP-INCREASE-KEY`；  
  本实现直接插入 key 并上浮，语义等价。

## 构建与测试

```powershell
mingw32-make ch06
mingw32-make test-ch06
.\build\ch06_heapsort\demo_heapsort.exe
```

## 阅读建议

1. 用 `build_max_heap` 后对照书中图 6.1 验证根为全局最大。
2. 想清楚堆排序为何**不稳定**（交换根与末尾）。
3. 练习：6.5-3 用优先队列做任务调度；可在本 lab 扩展。
