# 第 28 章：矩阵运算（Matrix Operations）

对应《算法导论》第三版第 28 章。实现 LUP 分解、解线性方程、行列式与求逆。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| LUP-DECOMPOSITION（部分主元） | 28.3 | `matrix_ops.c` | Θ(n³) |
| 解 A x = b | 28.3 | `matrix_ops.c` | Θ(n²) |
| 行列式 | 28.1 | `matrix_ops.c` | Θ(n³) |
| 矩阵求逆 | 28.1 | `matrix_ops.c` | Θ(n³) |

## 实现说明

- L 为单位下三角（对角 1 不显式存）、U 为上三角；`perm` 为行置换 π。
- 行列式 = sign(π) × Π U_ii；奇异时返回 0。
- 求逆：对每个单位向量 e_j 解 A x = e_j，拼成 A^{-1}。

## 构建与测试

```powershell
mingw32-make ch28
mingw32-make test-ch28
.\build\ch28_matrix_operations\demo_matrix_ops.exe
```

## 阅读建议

1. 为何需要部分主元（避免除零、改善数值稳定）。
2. 置换对行列式符号的影响。
3. 与 Strassen（ch04）在乘法上的用途区分。
