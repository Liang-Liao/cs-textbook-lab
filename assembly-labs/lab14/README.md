# Lab 14: PE 可执行文件剖析

## 目标
- 理解 Windows 可执行文件格式 PE/COFF
- 学会用 objdump / nm 分析二进制
- 建立 ELF ↔ PE 的概念映射

## Windows 是 PE，不是 ELF

| | Linux | Windows |
|--|-------|---------|
| 格式 | ELF (Executable and Linkable Format) | PE/COFF (Portable Executable) |
| 分析工具 | readelf | **objdump -x**（readelf 不认 PE） |
| 头部 | ELF Header → Program/Section Headers | DOS Header → PE Header → 节表 |
| 代码/数据节 | .text .data .rodata .bss | .text .data .rdata .bss |
| 动态库 | .so，符号直接可见 | .dll，经**导入表** (.idata) 引用 |

## PE 文件结构

```
┌──────────────────┐
│ DOS Header       │  'MZ' 魔数 (兼容历史)
│ DOS Stub         │  "This program cannot be run in DOS mode"
├──────────────────┤
│ PE Header        │  'PE\0\0' 魔数、机器类型、入口点 (AddressOfEntryPoint)
├──────────────────┤
│ Section Headers  │  .text / .data / .rdata / .bss 的位置和属性
├──────────────────┤
│ .text            │  代码段
│ .rdata           │  只读数据 (字符串常量、导入表)
│ .data            │  已初始化数据
│ .idata           │  导入表: 程序用到哪些 DLL 的哪些函数
└──────────────────┘
```

## 分析命令

```bash
mingw32-make build

objdump -f mini.exe    # 文件格式、入口点
objdump -h mini.exe    # 节表: 每节的大小、VMA、文件偏移
objdump -x mini.exe    # 全部: PE 头 + 节 + 符号 + 导入表
objdump -d mini.exe    # 反汇编
nm mini.exe            # 符号表

# 或者一键看全部重点
mingw32-make analyze
```

## 练习

### 练习 1: 运行 + 剖析
```bash
mingw32-make build
mingw32-make run       # 先看程序行为
mingw32-make analyze   # 再拆开看结构
```

### 练习 2: 找导入表
在 `objdump -x` 输出里找 "The Import Tables"：程序从哪些 DLL 导入了
哪些函数？ puts/printf 来自哪个 DLL？

### 练习 3: 对比 .text 大小
比较 `objdump -h` 里 .text 的实际大小和 `objdump -d` 反汇编出来的
代码长度，理解节大小为何略大（对齐填充）

## 验证

```bash
mingw32-make build && mingw32-make run
```

## 练习参考答案

> ⚠️ 剧透警告：先自己动手，再看答案！

### 练习 2
`objdump -x mini.exe | grep -A 10 "Import Tables"` 可以看到：
- `KERNEL32.dll` — 进程/内存/运行时基础设施
  - `__acrt_iob_func`（stdin/stdout 的 FILE* 获取）
  - `_initterm`、`__C_specific_handler` 等 CRT 启动支撑
- `api-ms-win-crt-*` 系列（UCRT，Windows 10+ 的统一 C 运行时）
  - `puts`、`printf` 就在这里

这解释了为什么我们能在汇编里直接 `call printf`——链接器把 UCRT
的导入桩写进了你的 exe。

### 练习 3
`objdump -h` 显示 .text 的大小是 16 的倍数（如 0x600）——节大小按
`FileAlignment`（通常 512）和 `SectionAlignment`（通常 4096）向上取整，
代码之间的空隙是 0x00/nop 填充。VMA 也按 SectionAlignment 对齐。
