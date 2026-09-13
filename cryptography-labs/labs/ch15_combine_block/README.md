# Lab 15 — 第15章 组合分组算法

对应书中**第 15 章 Combining Block Algorithms**。

## 实验目标

- 区分双重加密、三重 EDE
- 会解释中间相遇：时间从 \(2^{2k}\) 降到约 \(2^{k}\log 2^{k}\)（本实现排序+二分），代价是存储

## 文件

| 文件 | 内容 |
|------|------|
| `mini_cipher.c/.h` | 本地 64-bit toy Feistel（独立） |
| `combine.c/.h` | double / triple EDE / MITM demo |
| `test_ch15.c` | 回环与 12-bit MITM |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 对比 `double_encrypt` 与单次加密的密文
2. 把 MITM `key_bits` 从 10 调到 13，感受时间/内存增长
3. 思考为何现实用 3DES-EDE 而不是 2DES

## 思考题

- 中间相遇需要多大表？对 56-bit DES 意味着什么？
- 三重加密有效密钥长度是多少？（56×3 还是更少？）
- 与 AES-256 直接使用相比，组合旧算法有何缺点？

## 现代对照

3DES 已弃用；直接用 AES-256-GCM。
