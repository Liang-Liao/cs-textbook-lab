# 第 16 章：摊还分析（Amortized Analysis）

对应《算法导论》第三版摊还分析一章（英文版 Ch.17）。以动态表为例。

## 本章内容

| 算法/结构 | 书中节号 | 源文件 |
|-----------|----------|--------|
| 动态表扩容 | 17.2 | `dynamic_table.c` |
| 动态表扩缩容 | 17.3 | `dynamic_table.c` |

## 实现说明

- `push`：满时倍增；`pop`：当 `n ≤ size/4` 且 `size ≥ 2` 时收缩一半。
- 记录 `resizes` 为元素搬运次数；`n` 次 push 总搬运为 `n-1`（级数 1+2+…+n/2）。
- 摊还分析三种方法：聚合、记账、势能——本 lab 用实验验证聚合界 O(1) 每次插入。

## 构建与测试

```powershell
mingw32-make ch16
mingw32-make test-ch16
.\build\ch16_amortized\demo_amortized.exe
```

## 阅读建议

1. 为什么扩容摊还 O(1)，但单次最坏是 O(n)？
2. 扩缩容阈值 1/4 如何避免 thrashing（振荡）。
3. 势能法：Φ = 2n − size 在扩容/常规插入下的变化。
