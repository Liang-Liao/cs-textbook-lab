# 第 2 章：初识算法（Getting Started）

对应《算法导论》第三版第 2 章及 2.3 节。本 lab 是后续各章的**模板**。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| 插入排序 | 2.1 | `insertion_sort.c` | 最好 Θ(n)，平均/最坏 Θ(n²) |
| 归并排序 | 2.3 | `merge_sort.c` | Θ(n log n) |
| 计时对比 | — | `bench_sort.c` | 使用 `timing.h` |

## 与伪码的差异

- 书中数组下标从 **1** 开始，C 实现从 **0** 开始。
- 归并的辅助数组在 `merge_sort_int` 内部分配；`merge_int` 也可单独使用（调用方提供 `aux`）。

## 构建与测试

在仓库根目录：

```powershell
mingw32-make ch02
mingw32-make test-ch02
```

或在本目录：

```powershell
mingw32-make test
mingw32-make all
.\..\..\build\ch02_getting_started\demo_sort.exe
.\..\..\build\ch02_getting_started\bench_sort.exe   # 计时对比
```

## 阅读建议

1. 对照书中图 2.2 / 2.3 手推插入排序循环不变式。
2. 读懂归并的分治结构：分解 → 解决 → 合并。
3. 练习：2.1-4（二进制相加）、2.3-5（递归二分查找）可加到本 lab 作为扩展。
