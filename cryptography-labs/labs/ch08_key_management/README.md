# Lab 08 — 第8章 密钥管理

对应书中**第 8 章 Key Management**。

## 实验目标

- 理解 KEK（密钥加密密钥）/ DEK（数据加密密钥）分层：为何不直接长期存业务密钥
- 会做玩具 `wrap`/`unwrap`，并叠加**过期**与**吊销**检查
- 意识到吊销列表（CRL）与证书透明度要解决什么运维问题

## 文件

| 文件 | 内容 |
|------|------|
| `keymgr.c/.h` | 密钥仓库、角色、过期、吊销、XOR+mix64 wrap |
| `test_ch08.c` | wrap/unwrap 回环、吊销拒绝、过期拒绝 |

> wrap 用 `dek ⊕ mix64(kek)`，**仅教学**。

## 构建与测试

```powershell
mingw32-make -C ..\..\common
mingw32-make test
```

## 动手实验

1. 去掉 `km_wrap` 里 `km_usable(dek)` 检查，再跑吊销用例，应「错误地成功」
2. 给 KEK 设很短的 `not_after`，观察在到期后 unwrap 失败
3. 把 DEK 误标成 `KM_KEK`，确认 wrap 因角色不匹配失败

## 思考题

- 为何要分层？轮换 KEK 时 DEK 要不要全部重加密？
- 在线吊销（OCSP）和离线 CRL 的取舍？
- 证书透明度日志如何补 CRL 的缺口？

## 现代对照

现实用 HSM/KMS、AES key wrap (RFC 3394)、X.509 扩展、CT log。
