# 第 8 章：线性时间排序（Sorting in Linear Time）

对应《算法导论》第三版第 8 章。实现计数排序、基数排序与桶排序。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| COUNTING-SORT | 8.2 | `counting_sort.c` | Θ(n+k)，稳定 |
| RADIX-SORT | 8.3 | `radix_sort.c` | Θ(d(n+k))，按位稳定 |
| BUCKET-SORT | 8.4 | `bucket_sort.c` | 期望 Θ(n)，[0,1) 浮点 |

## 实现说明

- 计数排序：输入键 ∈ `[0,k]`，输出写入独立数组 `b`，源数组不变。
- 基数排序：默认十进制位；可用 `radix_sort_int_base` 换进制；要求非负整数。
- 桶排序：键 ∈ `[0,1)`，桶下标 `floor(n*x)`，桶内插入排序。

## 构建与测试

```powershell
mingw32-make ch08
mingw32-make test-ch08
.\build\ch08_linear_sort\demo_linear_sort.exe
```

## 阅读建议

1. 为什么比较排序下界 Ω(n lg n) 不适用于这些算法（8.1 节）？
2. 计数排序稳定性从何而来（逆序扫描）？
3. 练习：8.3-5 用 0/1 键证明基数排序需稳定中间排序。
