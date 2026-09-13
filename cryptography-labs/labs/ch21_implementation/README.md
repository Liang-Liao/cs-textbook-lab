# Lab 21 — 第21章 实现细节 / 填充 Oracle

对应书中**第 21 章**：实现陷阱。本 lab 演示 **Vaudenay 填充 oracle 攻击**。

## 实验目标

跑通后你能解释：为什么「填充是否合法」的旁路反馈足以逐字节解出整段明文，
以及为什么第一块要靠 IV 引导、最后要按 PKCS#7 截断。

## 文件

| 文件 | 内容 |
|------|------|
| `oracle.c/.h` | 本地 4 轮 Feistel + CBC、`oracle_padding_ok`（只回填充合法性）、`padding_oracle_attack` |
| `test_ch21.c` | 攻击恢复完整明文 + 篡改必失败 |

## 思想

若解密端对“填充是否合法”给出可区分反馈（错误码/耗时），攻击者可逐字节恢复明文。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 把 `oracle_padding_ok` 改成总是返回 1，攻击应失败
2. 在真实服务里：统一错误响应、先验 MAC 再解密（Encrypt-then-MAC）

## 现代对照

TLS 历史上的 Lucky Thirteen 等；现代用 AEAD。
