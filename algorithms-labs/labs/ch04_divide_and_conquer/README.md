# 第 4 章：分治策略（Divide-and-Conquer）

对应《算法导论》第三版第 4 章。实现书中最大子数组与矩阵乘法（含 Strassen）。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| 最大子数组（分治） | 4.1 | `max_subarray.c` | Θ(n log n) |
| 最大子数组（暴力） | 4.1 | `max_subarray.c` | Θ(n²)，用于对照 |
| 最大子数组（Kadane） | 练习扩展 | `max_subarray.c` | Θ(n) |
| 朴素矩阵乘法 | 4.2 | `matrix_multiply.c` | Θ(n³) |
| 递归矩阵乘法 | 4.2 | `matrix_multiply.c` | Θ(n³)，结构演示 |
| Strassen | 4.2 | `matrix_multiply.c` | Θ(n^{lg 7}) |

## 实现说明

- 最大子数组：0-based 下标；书中 1-based 的 A[8..11] 对应本实现 `[7..10]`。
- 递归矩阵乘法：偶数 n 才真正四分递归；奇数 n 或很小的 n 回退到朴素法。
- Strassen：要求 n 为 2 的幂；很小的块走朴素法以降低常数开销。

## 构建与测试

```powershell
mingw32-make ch04
mingw32-make test-ch04
.\build\ch04_divide_and_conquer\demo_divide_conquer.exe
```

## 阅读建议

1. 对照书中图 4.1，手推 `max_crossing` 为何必须「跨中点」才能合并左右最优。
2. 写出递归式 T(n)=2T(n/2)+Θ(n) 并用主方法得到 Θ(n log n)。
3. 比较三次方递归与 Strassen 的 7 次子乘积；理解为何工程上仍常需分块/缓存优化。
4. 练习：4.1-2 暴力最大子数组已实现；可再做 4.1-5 非递归 Kadane 的循环不变式笔记。
