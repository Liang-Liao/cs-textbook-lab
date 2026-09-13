# Lab 12 — 第12章 DES

对应书中**第 12 章 Data Encryption Standard**。

## 实验目标

- 对照 IP/FP、扩展 E、S 盒、P 置换、16 轮 Feistel
- 理解 PC-1/PC-2 与 56-bit 有效密钥
- 会识别弱密钥/半弱密钥

## 文件

| 文件 | 内容 |
|------|------|
| `des.c/.h` | 教育版 DES（比特表驱动，便于对照教材） |
| `test_ch12.c` | FIPS 经典向量、回环、弱密钥 |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

已知向量示例：

- key = `0123456789ABCDEF`, pt = `0123456789ABCDEF` → `56CC09E7CFDC4CEF`
- key = 0, pt = 0 → `8CA64DE9C1B123A7`

## 动手实验

1. 在 `des_f` 的 S 盒输出后打印 32-bit，对照书上轮结构
2. 用弱密钥加密一次，验证 `E(E(P)) = P`
3. 改一个密钥 bit，观察密文雪崩

## 思考题

- 为何 DES 的安全性讨论集中在 S 盒？
- 56-bit 密钥今天暴力搜索需要多久？
- 与 AES 的结构差异（SPN vs Feistel）？

## 现代对照

DES/3DES 已废弃；用 AES-128/256。本实现仅教学。
