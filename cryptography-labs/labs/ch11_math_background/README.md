# Lab 11 — 第11章 数学背景

对应书中**第 11 章 Mathematical Background**。

## 实验目标

- 会算欧拉函数 φ(n)，并联系 RSA 的 φ(n)=(p-1)(q-1)
- 会用中国剩余定理解同余方程组（RSA CRT 加速的数学基础）
- 会做 AES 域 GF(2^8) 的 xtime / 乘法 / 求逆

## 文件

| 文件 | 内容 |
|------|------|
| `numthy.c/.h` | `euler_phi_u64`、`crt_u64` |
| `gf2n.c/.h` | AES GF(2^8) 与 GF(2^32) 无进位乘 |
| `test_ch11.c` | 向量与不变量 |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

已知向量：`xtime(0x57)=0xAE`，`0x57*0x83=0xC1`（AES 文档）。

## 动手实验

1. 对 n=300 手算 φ，再对照 `euler_phi_u64`
2. 用 CRT 合并 `x≡2 (3)`、`x≡3 (5)` 得到 8，推广到三模数
3. 打印 `a * a^{-1}` 对多个 a 是否恒为 1

## 思考题

- 为什么 RSA 选 e 与 φ(n) 互素？
- AES 为何用 GF(2^8) 而不是整数模 256？
- 无进位乘与普通整数乘的本质差别？

## 现代对照

生产用完整 bignum；AES 硬件常加速 xtime/GF(2^8)。
