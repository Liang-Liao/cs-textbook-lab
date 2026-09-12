# 第 31 章：数论算法（Number-Theoretic Algorithms）

对应《算法导论》第三版第 31 章。实现模幂、欧几里得与扩展欧几里得、模逆、中国剩余。

## 本章算法

| 算法 | 书中节号 | 源文件 |
|------|----------|--------|
| MODULAR-EXPONENTIATION | 31.6 | `number_theory.c` |
| EUCLID | 31.2 | `number_theory.c` |
| EXTENDED-EUCLID | 31.3 | `number_theory.c` |
| 模逆 / 中国剩余 | 31.4 | `number_theory.c` |

## 实现说明

- 模幂用快速幂，模乘用倍加法（逐位翻倍取模），要求模数 m < 2^62 以免加法溢出 `int64`。
- 扩展欧几里得：`ax + by = gcd(a,b)`；模逆要求 `gcd=1`。
- 中国剩余：两两互素的模数，返回 `[0, nm)` 内唯一解。

## 构建与测试

```powershell
mingw32-make ch31
mingw32-make test-ch31
.\build\ch31_number_theory\demo_number_theory.exe
```

## 阅读建议

1. 模幂为何是 RSA 与 Diffie-Hellman 的核心。
2. 扩展欧几里得的递归结构与系数回代。
3. 中国剩余在 RSA 解密加速中的应用。
