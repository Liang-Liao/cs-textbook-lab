# Lab 19 — 第19章 公钥算法

对应书中**第 19 章**：RSA、Diffie-Hellman。

## 实验目标

- 手推 `c = m^e mod n`，`m = c^d mod n`
- 验证 DH 双方得到相同共享秘密
- 理解教学填充与真实 PKCS#1/OAEP 的差距

## 文件

| 文件 | 内容 |
|------|------|
| `rsa.c/.h` | 小模数 RSA（uint64） |
| `dh.c/.h` | DH（演示素数 \(2^{31}-1\)） |
| `test_ch19.c` | 回环与一致性 |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 固定一对小素数，打印 `n, φ(n), e, d` 并验 `e*d ≡ 1 (mod φ)`
2. 篡改 DH 一方私钥，确认共享秘密不一致
3. 用 1-byte 消息跑 `rsa_public_bytes`，观察填充字节

## 思考题

- 为何教科书 RSA（无填充）不安全？
- DH 如何抵抗窃听？又如何被中间人？
- 今天 RSA 的建议参数与替代方案（ECDH、Ed25519）？

## 现代对照

生产：RSA-2048+OAEP 或直接 ECC；DH 用 ECDH P-256/X25519。
