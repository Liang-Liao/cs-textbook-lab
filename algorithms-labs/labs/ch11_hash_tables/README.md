# 第 11 章：散列表（Hash Tables）

对应《算法导论》第三版第 11 章。实现直接寻址、链式散列与开放寻址。

## 本章内容

| 结构/算法 | 书中节号 | 源文件 |
|-----------|----------|--------|
| 直接寻址表 | 11.1 | `direct_address.c` |
| 除法/乘法散列 | 11.3 | `hash_chain.c` |
| 链式散列表 | 11.2 | `hash_chain.c` |
| 开放寻址（线性探测 + 墓碑） | 11.4 | `hash_open_address.c` |

## 实现说明

- 直接寻址：值 `0` 表示空槽，插入值须非 0。
- 链式：头插；`search` 返回链上第一个匹配。
- 开放寻址：`h'(k)=k mod m`，`h(k,i)=(h'(k)+i) mod m`；删除用墓碑  
  （`OPEN_DELETED_KEY`）；空槽哨兵为 `OPEN_EMPTY_KEY`（`INT_MIN`），  
  这两个哨兵值不可用作合法 key。

## 构建与测试

```powershell
mingw32-make ch11
mingw32-make test-ch11
.\build\ch11_hash_tables\demo_hash_tables.exe
```

## 阅读建议

1. 装载因子 α 如何影响链式查找与开放寻址的期望代价？
2. 为什么开放寻址删除需要墓碑而不能简单置空？
3. 练习：11.4-4 聚集（clustering）与二次探测/双重散列。
