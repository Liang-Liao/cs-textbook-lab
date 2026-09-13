# Lab 16 — 第16章 流密码与伪随机

对应书中**第 16 章**：LFSR、RC4。

## 实验目标

- 理解最大长度 LFSR 周期 \(2^n-1\)
- 认识 LFSR 的**线性**弱点（可被代数恢复初态）
- 会写 RC4 的 KSA/PRGA 并核对公开 KAT

## 文件

| 文件 | 内容 |
|------|------|
| `lfsr.c/.h` | 16/32-bit LFSR |
| `rc4.c/.h` | RC4 |
| `test_ch16.c` | 周期、RC4 KAT |

RC4 KAT：key=`Key`, pt=`Plaintext` → `BBF316E8D940AF0AD3`

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 改 LFSR taps 为非本原多项式，观察周期变短
2. 输出 16 个 LFSR bit，尝试手工解线性方程组恢复 state
3. 丢弃 RC4 前 256 字节输出（Fluhrer-Mantin-Shamir 的历史教训）

## 思考题

- 为何“长周期”≠“密码安全”？
- RC4 今天还在哪里合法使用？（基本不应再用）
- 现代流密码 ChaCha20 如何避免 LFSR 线性性？

## 现代对照

RC4 已禁用；用 ChaCha20 / AES-CTR + HMAC 或 AEAD。
