# 第 30 章：多项式与快速傅里叶变换（Polynomials and the FFT）

对应《算法导论》第三版第 30 章。实现朴素多项式乘法与 FFT 卷积。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度 |
|------|----------|--------|--------|
| 朴素多项式乘法 | 30.1 | `poly_fft.c` | Θ(n²) |
| 迭代 Cooley-Tukey FFT / 逆 FFT | 30.5–30.6 | `poly_fft.c` | Θ(n lg n) |
| FFT 卷积 | 30.2 | `poly_fft.c` | Θ(n lg n) |

## 实现说明

- 系数表示 `a[0..n-1]` 对应 \(a_0 + a_1 x + \cdots\)；`n` 须为 2 的幂。
- FFT 就地迭代实现（位逆序 + 蝶形）；卷积：补零到 `2n` → FFT → 点乘 → 逆 FFT。
- 测试与朴素结果逐系数比较（容差 `1e-6`）。

## 构建与测试

```powershell
mingw32-make ch30
mingw32-make test-ch30
.\build\ch30_polynomials_fft\demo_fft.exe
```

## 阅读建议

1. 系数表示 vs 点值表示；在单位根上求值即 DFT。
2. 逆 DFT 为何是共轭再除以 n。
3. 大整数乘法可化为多项式卷积（书中应用）。
