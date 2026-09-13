# Lab 03 — 第3章 基础协议

对应书中**第 3 章 Basic Protocols**：挑战-响应认证。

## 实验目标

- 理解共享秘密下如何证明“我知道密钥”而不传密钥
- 体会重放攻击：同一挑战可被再次使用（本实现未加时间戳/序列号）

## 文件

| 文件 | 内容 |
|------|------|
| `protocols.c/.h` | `auth_challenge` / `auth_response` / `auth_verify` |
| `test_ch03.c` | 正确/错误密钥、错误响应 |

单向部件使用 `common` 的 `mix64_u64`（教学占位，非密码哈希）。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 先截获一对 `(challenge, response)`，再次提交——仍会通过，说明需要防重放
2. 把 `h2` 换成 `SHA256`（可临时把 ch18 的文件拷进本 lab 再改）观察变化
3. 画出 A→B 的消息序列图

## 思考题

- 为什么不直接传口令哈希？
- 若没有可信第三方，中间人如何破坏此协议？
- 书中后续时间戳/nonce 方案分别解决什么问题？

## 现代对照

现代用 TLS 握手、PAKE、HOTP/TOTP；认证码用 HMAC-SHA256。
