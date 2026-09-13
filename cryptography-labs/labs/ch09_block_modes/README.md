# Lab 09 — 第9章 算法类型与模式

对应书中**第 9 章**：ECB / CBC / CFB / OFB / CTR。

## 实验目标

- 会解释 ECB 为何泄漏明文结构
- 会手推 CBC 的 `C_i = E(P_i XOR C_{i-1})`
- 理解 OFB/CTR 是流密码化使用分组密码

## 文件

| 文件 | 内容 |
|------|------|
| `modes.c/.h` | 五种工作模式 |
| `toy_cipher.c/.h` | 独立 64-bit Feistel（本 lab 不依赖 DES） |
| `test_ch09.c` | 回环、ECB 泄漏、CBC 错误传播 |

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 把两个明文块改成相同，看 ECB 密文块是否相同、CBC 是否不同
2. 翻转密文第 1 字节 1 bit，看 CBC 明文块 0 全乱、块 1 仅 1 bit 变
3. 改 IV 后重新 CBC 加密，比较第一密文块

## 思考题

- 为什么 CTR 可并行而 CBC 加密不能？
- CFB-8 适合什么场景？
- 需要完整性时为什么不能只靠这些模式？（→ 需 AEAD/MAC）

## 现代对照

生产首选 AES-GCM / ChaCha20-Poly1305，不再单独用裸 CBC/ECB。
