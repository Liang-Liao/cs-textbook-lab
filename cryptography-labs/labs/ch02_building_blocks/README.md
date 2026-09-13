# Lab 02 — 第2章 协议构件

对应 Schneier《应用密码学》**第 2 章 Protocol Building Blocks**。

## 实验目标

- 会用模乘/模幂/模逆解释协议里的“难问题”
- 会跑 Miller-Rabin 素性测试
- 体会单向函数：确定、雪崩、难求逆

## 文件

| 文件 | 内容 |
|------|------|
| `oneway.c/.h` | 教学用 Davies-Meyer 单向哈希原型（256-bit） |
| `test_ch02.c` | 数论构件（common）+ oneway 测试 |

模运算/素性等原语在 `common/`（本 lab 调用并测）。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 改 `oneway()` 输入 `abc` → `abd`，对比 32 字节摘要差异比例
2. 把 `random_prime_u64` 范围改到 `10^6` 量级，观察生成耗时
3. 阅读 `mod_pow_u64` 的俄罗斯农民乘法，手推 `2^10 mod 1000`

## 思考题

- 为什么协议要建立在“求逆难”而不是“加密看起来乱”上？
- 单向哈希与可逆混合（`ow_mix32`）的本质区别？
- Miller-Rabin 的错误概率与 `rounds` 的关系？

## 现代对照

真实系统用 SHA-2/SHA-3、bignum（GMP/OpenSSL BN），模数位长 ≥ 2048-bit。
