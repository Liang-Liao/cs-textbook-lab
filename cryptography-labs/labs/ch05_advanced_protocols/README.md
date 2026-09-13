# Lab 05 — 第5章 高级协议

对应书中**第 5 章 Advanced Protocols**。

## 实验目标

- 理解哈希承诺 `c = H(rlen‖r‖mlen‖m)` 的「绑定 + 隐藏」结构（长度前缀防拼接歧义）
- 走通 Fiat-Shamir 风格的离散对数知识证明：证明知道 `x` 使 `X = g^x (mod p)` 而不泄露 `x`
- 会验证 `g^z ≡ A · X^c (mod p)`

## 文件

| 文件 | 内容 |
|------|------|
| `advproto.c/.h` | `commit`/`open_check`、`zk_prove`/`zk_verify` |
| `sha256.c/.h` | 本地哈希（零跨 lab 依赖） |
| `test_ch05.c` | 承诺开合、ZK 接受/拒绝 |

> 小模数、非生产实现，仅教学。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 同一 `r,m` 跑两次 `commit`，结果应相同；改 `m` 后打开应失败
2. 把 `zk_verify` 里左右两边对调，确认证明变失败
3. 固定 `r` 为常量，观察证明是否仍可通过（随机性来自 `r`）

## 思考题

- 承诺若只用 `H(m)`（无随机数 `r`）会怎样？
- Fiat-Shamir 变换把交互式协议变成非交互式的前提是什么？
- 现实中的 Schnorr/EdDSA 与这个玩具差在哪里？

## 现代对照

现实用 Schnorr 签名、Ed25519、zk-SNARK；承诺用 Pedersen。
