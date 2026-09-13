# Lab 04 — 第4章 中间协议

对应书中**第 4 章 Intermediate Protocols**。

## 实验目标

- 理解 Shamir \((k,n)\) 门限：任意 \(k\) 份可恢复秘密，少于 \(k\) 份信息不足
- 体会 Kerberos 式票据：用服务端密钥“包住”会话密钥
- 理解时间戳链如何把事件顺序钉死

## 文件

| 文件 | 内容 |
|------|------|
| `secret_share.c/.h` | GF(p) 上 Shamir 分裂/拉格朗日恢复 |
| `ticket.c/.h` | 简化票据加解密 + 时间戳哈希链 |
| `test_ch04.c` | 门限、票据、链完整性 |

> 票据/XOR 流与 `mix64` **仅教学**，不是密码学安全实现。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 用同一秘密跑两次 `ss_split`，对比 share 是否相同（应不同）
2. 只给 2 份 share 调 `ss_recover`，确认失败
3. 篡改时间戳链某条 data，确认 `ts_chain_check_link` 失败

## 思考题

- 为什么 Shamir 要求系数随机且定义在有限域上？
- 票据中若会话密钥明文放进网络会怎样？
- 哈希链与数字签名在“防篡改”上的差别？

## 现代对照

现实用 Shamir 库（大素数/字节秘密）、Kerberos AS-REQ/AS-REP、RFC3161 时间戳或区块链式日志。
