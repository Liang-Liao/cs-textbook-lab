# Lab 22 — 第22章 例题与综合方案

对应书中**第 22 章**：把口令、KDF、加密、认证拼成一个「迷你信封」。

## 实验目标

- 走通 **口令 → PBKDF2 → enc key ‖ mac key** 的密钥派生
- 理解 **encrypt-then-MAC**：先加密再对密文（+盐）做 HMAC，篡改必被拒
- 体会随机 salt 使同一明文多次封印得到不同密文

## 文件

| 文件 | 内容 |
|------|------|
| `envelope.c/.h` | `env_seal` / `env_open` |
| `sha256.c/.h` | 本地哈希（HMAC/PBKDF2 原料） |
| `test_ch22.c` | 回环、错口令、篡改、salt 差异 |

封印布局：`salt[8] || HMAC-SHA256[32] || ciphertext[]`  
密文 = PKCS#7(8) 后与 `mix64` 密钥流 XOR。**仅教学**，不可用于生产。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 改 `env_seal` 的 PBKDF2 迭代从 1000 到 1，观察 `test` 仍过但明显更弱
2. 在 `env_open` 里跳过 MAC 检查，再跑篡改用例，应变成「通过」——说明 MAC 不可省
3. 把 salt 改成全零常量，对比两次 seal 的密文是否相同

## 思考题

- 为什么 MAC 密钥要与加密密钥分开派生？
- encrypt-then-MAC vs MAC-then-encrypt，攻击面差在哪？
- 真实产品里这个信封还缺什么（AAD、版本字节、KDF 参数存储、nonce）？

## 现代对照

现实用 AES-GCM / ChaCha20-Poly1305；口令哈希用 Argon2/scrypt/PBKDF2-HMAC-SHA256 并存参数与盐。
