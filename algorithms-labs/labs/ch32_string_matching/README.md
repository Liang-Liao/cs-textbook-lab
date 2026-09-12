# 第 32 章：字符串匹配（String Matching）

对应《算法导论》第三版第 32 章。实现朴素、Rabin-Karp 与 KMP。

## 本章算法

| 算法 | 书中节号 | 源文件 | 复杂度（期望/最坏） |
|------|----------|--------|----------------------|
| 朴素匹配 | 32.1 | `string_match.c` | O((n−m+1)m) |
| Rabin-Karp | 32.2 | `string_match.c` | 期望 O(n+m)，伪命中再比对 |
| KMP + 前缀函数 | 32.4 | `string_match.c` | O(n+m) |

## 实现说明

- 匹配位置为 **0-based** 起始下标（书中 1-based 的 `s` 对应 `s-1`）。
- Rabin-Karp：`d=256`，模数 `q` 由调用方给出（测试用 101）。
- KMP 前缀函数：书中 `ababaca` → `π = 0 0 1 2 3 0 1`。

## 构建与测试

```powershell
mingw32-make ch32
mingw32-make test-ch32
.\build\ch32_string_matching\demo_string_match.exe
```

## 阅读建议

1. 朴素算法最坏在 `aaaa`/`aaa` 上的右移。
2. RK 的散列冲突与 `q` 选取。
3. KMP 自动机与 `π` 如何避免回溯文本指针。
