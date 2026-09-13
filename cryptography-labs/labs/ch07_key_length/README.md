# Lab 07 — 第7章 密钥长度

对应书中**第 7 章 Key Length**。

## 实验目标

- 会估算暴力搜索耗时：`2^{bits} / keys_per_sec` 并换算成年
- 理解生日界 ≈ `2^{b/2}` 对哈希/块长的意义
- 用 8-bit 玩具穷举体会「搜索空间」与「弱加密」

## 文件

| 文件 | 内容 |
|------|------|
| `keylen.c/.h` | `brute_force_years`、`birthday_trials`、`count_matching_keys` |
| `test_ch07.c` | 56-bit / 128-bit 耗时、64-bit 生日、玩具密钥搜索 |

玩具分组：对每字节加上低 8 位密钥（重复密钥），仅用于穷举演示。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 把 `keys_per_sec` 从 `1e9` 改到 `1e15`，对比 56-bit 与 128-bit 年数差几个数量级
2. 计算 `birthday_trials(128)`，体会「哈希 128-bit ≈ 只剩 64-bit 抗碰撞」
3. 改玩具 `enc8` 的运算（如异或），确认搜索仍能命中正确密钥

## 思考题

- 为什么抗原像用 2^n、抗碰撞用 2^{n/2}？
- 专用硬件（ASIC）如何改变「安全密钥长度」表？
- 今天推荐的对称/哈希最小密钥与摘要长度是多少？

## 现代对照

对称 ≥128-bit，哈希 ≥256-bit（抗碰撞）；GPU/ASIC 使 56-bit DES 完全不可用。
