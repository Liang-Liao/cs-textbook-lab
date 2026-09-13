# Lab 06 — 第6章 异常协议

对应书中**第 6 章 Esoteric Protocols**。

## 实验目标

- 理解心理扑克：双方各自「上锁 + 洗牌」，用**可交换锁**保证互不信任时仍能得到合法排列
- 理解同时签约：双方先互相**承诺**（书中的逐 bit 承诺在本 lab 简化为对整段文本的单个 SHA-256 承诺，commit→open 流程相同），再交换 opening；单方违约时另一方仍能证明对方已被承诺绑定

## 文件

| 文件 | 内容 |
|------|------|
| `poker.c/.h` | 52 张牌、可交换加锁（mod 52）、双向洗牌协议 |
| `contract.c/.h` | SHA-256 承诺、双方 commit→open 会话、违约证明 |
| `sha256.c/.h` | 本地哈希（零跨 lab 依赖） |
| `test_ch06.c` | 排列合法性、锁可交换、签约完成与违约检测 |

> 洗牌用确定性 LCG、锁用加法 mod 52，**仅教学**，不是密码学安全实现。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 令 Alice/Bob 使用相同 seed 以外的不同 key，连续跑 10 次 `poker_two_party_shuffle`，确认始终是 0..51 排列
2. 在 `contract_exchange_open` 里只打开 `'A'`，确认 `contract_exchange_complete` 为假
3. 篡改 Bob 的 `text` 后再 `contract_prove_bound`，应失败

## 思考题

- 若锁不可交换（如普通 AES），双方如何盲洗？需要同态/可交换加密吗？
- 同时签约里，若 A 先 opening 而 B 消失，A 的“损失”是什么？公平交换还缺什么？
- 真实 mental poker 为何需要牌面保密而不只是顺序保密？

## 现代对照

现实用可验证洗牌（cut-and-choose / 零知识）或可信执行环境；签约用带时间戳的双人签名或公证人。
