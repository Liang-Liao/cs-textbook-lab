# 第 35 章：近似算法（Approximation Algorithms）

对应《算法导论》第三版第 35 章。实现顶点覆盖 2-近似与度量 TSP 2-近似。

## 本章算法

| 算法 | 书中节号 | 源文件 | 近似比 |
|------|----------|--------|--------|
| 顶点覆盖（贪心取边两端点） | 35.1 | `approx.c` | ≤ 2 |
| 度量 TSP（MST + 先序捷径） | 35.2 | `approx.c` | ≤ 2（度量假设） |

## 实现说明

- 顶点覆盖：取一条未覆盖边的两个端点，删除所有关联边；覆盖必成立，大小 ≤ 2·OPT。
- TSP：Prim 生成 MST，DFS 先序得到访问序列，回路按度量边权求和。

## 构建与测试

```powershell
mingw32-make ch35
mingw32-make test-ch35
.\build\ch35_approximation\demo_approx.exe
```

## 阅读建议

1. NP 难问题为何用近似比衡量。
2. 度量 TSP 的三角不等式如何保证 MST+捷径 ≤ 2·OPT。
3. 集合覆盖的 ln n 近似（书中 35.3）可作扩展。
