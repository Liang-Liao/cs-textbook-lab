# Lab 13 — 第13章 FEAL-8（结构教学版）

对应书中**第 13 章 Other Block Algorithms**。

## 说明

FEAL 曾是 DES 的“更快替代”，后被差分密码分析击破。本 lab 实现 **FEAL 风格** 的
32-bit Feistel + S0/S1 与简化密钥编排，用于理解结构与失败史，**不是**兼容官方 FEAL 的实现。

f 轮函数使用输入的全部 4 个字节（S0/S1 级联 + 回混），与真 FEAL 的思路一致；
但级联细节、密钥编排与本 lab 简化版不同，因此没有官方 KAT，正确性靠加解回环验证。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 思考题

- 为何 FEAL 比 DES 更容易差分分析？
- 轮数不够会怎样？
- 现代分组密码如何设计抵抗差分/线性分析？

## 现代对照

FEAL 已废弃；用 AES。
