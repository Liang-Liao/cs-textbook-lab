# Lab 18 — 第18章 单向哈希函数

对应书中**第 18 章**：MD5、SHA-1、SHA-256。

## 实验目标

- 会对照 Merkle-Damgård 填充与长度编码
- 用 RFC/FIPS 向量验证实现
- 理解雪崩效应

## 文件

| 文件 | 内容 |
|------|------|
| `md5.c/.h` | MD5 |
| `sha1.c/.h` | SHA-1 |
| `sha256.c/.h` | SHA-256 |
| `test_ch18.c` | 官方向量 |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 对 `""` / `"a"` / `"abc"` 打印摘要，对照 RFC
2. 修改 MD5 消息末尾 1 bit，统计摘要汉明距离
3. 阅读 `*_final` 的 padding，手算 `len("abc")=24` 如何进入最后一块

## 思考题

- 长度扩展攻击为何对裸 MD 结构成立？
- SHA-1 碰撞对证书签名意味着什么？
- SHA-3（海绵结构）与 MD 的差异？

## 现代对照

签名/完整性用 SHA-256/384/3 或 BLAKE3；MD5/SHA-1 仅保留兼容用途。
