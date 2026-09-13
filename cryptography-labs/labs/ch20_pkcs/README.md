# Lab 20 — 第20章 PKCS

对应书中**第 20 章 PKCS**：填充与口令派生。

## 实验目标

跑通后你能解释：PKCS#7 填充为什么必须「整块填充」且逐字节校验；
PBKDF2 如何把口令拉伸成密钥（HMAC 作 PRF、盐防彩虹表、迭代防暴力）。

## 文件

| 文件 | 内容 |
|------|------|
| `pkcs.c/.h` | `pkcs7_pad` / `pkcs7_unpad`、`pbkdf2_hmac_sha256` |
| `sha256.c/.h` | 本地内嵌 SHA-256（不依赖 ch18） |
| `test_ch20.c` | 官方向量（password/salt, c=1,2,4096；dkLen=40）+ 填充负例 |

## 内容

- PKCS#7 pad / unpad（含非法填充拒绝）
- PBKDF2-HMAC-SHA256（本地内嵌 SHA-256，不依赖 ch18）

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

PBKDF2 向量来自公开测试（password/salt, c=1,2,4096 与长口令/长盐、dkLen=40 的标准向量）。

## 动手实验

1. 把 `pkcs7_unpad` 的填充校验改成只看最后一个字节，构造一个会被误接受的畸形输入
2. 把 PBKDF2 迭代次数从 1000 提到 100000，计时对比
3. 用相同口令、不同盐派生两次，确认密钥不同

## 思考题

- 为什么解密端必须严格校验填充？（填充 oracle）
- PBKDF2 迭代次数今天该设多少？与 bcrypt/scrypt/Argon2 对比？
- PKCS#1 v1.5 与 OAEP 的差别？
