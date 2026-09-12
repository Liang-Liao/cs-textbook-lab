# x86-64 汇编语言实战 Lab 课程

> 从零到能读懂内核代码，16 个递进式实验（Windows MSYS2 UCRT64，16/16 自动化测试通过）

## 为什么学这个？

教科书教 16 位 DOS 汇编（王爽《汇编语言》），但你实际看到的内核、编译器输出、逆向工程全是 **x86-64**。本课程直接在现代环境下学，每个 Lab 都能跑、能调试、能和 C 互操作。

## 平台

| 平台 | 工具链 | 状态 |
|------|--------|------|
| **Windows** (MSYS2 UCRT64) | gcc + binutils + gdb | ✅ 全部构建运行并通过 `mingw32-make test` |

课程以 **Microsoft x64 ABI** 为唯一教学约定（参数 RCX/RDX/R8/R9、shadow space、
16 字节栈对齐、RDI/RSI 为 callee-saved、long 是 32 位），细节见 `WINDOWS.md`。
如果你在 Linux/WSL 工作，本课程也是很好的对照材料——每个 ABI 差异点
（参数寄存器、浮点传参、RDI/RSI 归属）在对应 lab 里都有标注。

## 环境准备

打开 **MSYS2 UCRT64** 终端（不是 MSYS2，不是 MINGW64）：

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils mingw-w64-ucrt-x86_64-make
```

详细说明与 ABI 速查见 `WINDOWS.md`。lab16 额外需要 QEMU。

## 工具链

| 工具 | 用途 |
|------|------|
| **gcc** | 编译汇编 + 链接 C 运行时 |
| **as** | GNU 汇编器（AT&T 语法） |
| **gdb** | 调试器（`layout asm` 看汇编） |
| **objdump** | 反汇编 / PE 文件剖析（Windows 没有 readelf） |
| **mingw32-make** | 构建自动化 |

## 语法说明

本课程使用 **GAS (AT&T) 语法**，通过 **gcc** 编译链接。

### AT&T vs Intel 语法对比

| 特性 | AT&T (GAS) | Intel (NASM) |
|------|-----------|-------------|
| 寄存器前缀 | `%rax` | `rax` |
| 立即数前缀 | `$42` | `42` |
| 操作数顺序 | `mov src, dst` | `mov dst, src` |
| 内存寻址 | `offset(%base,%idx,scale)` | `[base+idx*scale+offset]` |
| 指令后缀 | `movq` (64位) | `mov` (由操作数推断) |

**为什么选 AT&T + gcc？** GCC/objdump 输出都是 AT&T 风格，学一套看两处；
通过 gcc 链接可以自动处理 C 运行时，代码中直接调用 `printf`/`puts` 做输出。

## Lab 索引

| 阶段 | Lab | 主题 | 自动测试 |
|------|-----|------|----------|
| **基础** | 01 | Hello World — 程序结构、调用约定 | ✅ |
| | 02 | 寄存器与数据移动 — MOV、数据大小 | ✅ |
| | 03 | 算术运算 — ADD/SUB/MUL/DIV | ✅ |
| | 04 | 条件与跳转 — CMP/Jcc、分段函数 | ✅ |
| | 05 | 循环 — 计数器、斐波那契、**GAS 宏** | ✅ |
| **中级** | 06 | 栈与过程调用 — CALL/RET、递归 | ✅ |
| | 07 | 数组与字符串 — 寻址、REP MOVSB | ✅ |
| | 08 | 位运算 — AND/OR/XOR/SHL、技巧 | ✅ |
| | 09 | 浮点运算 — SSE 浮点 + Windows 浮点传参 | ✅ |
| **高级** | 10 | C 与汇编互操作 — Windows x64 ABI 实战 | ✅ |
| | 11 | 文件 I/O — fopen/fprintf/fgets | ✅ |
| | 12 | SIMD 向量化 — SSE 并行 + 栈传参 | ✅ |
| **实战** | 13 | 性能优化 — 循环展开 + QPC 真实计时 | ✅ |
| | 14 | PE 文件结构 — objdump 剖析可执行文件 | ✅ |
| | 15 | Shellcode — 机器码就是字节 | ✅ |
| | 16 | 迷你 Bootloader — 引导扇区、实模式 | ✅ |

## 如何使用

```bash
# 进入某个 lab 目录
cd lab01

# 编译 / 运行 / GDB 调试
mingw32-make build
mingw32-make run
mingw32-make debug

# 一键构建全部 lab
mingw32-make all

# 一键自动验证 (与各 lab 的 expected.txt 比对)
mingw32-make test

# 查看某个 lab 的期望输出
cat lab05/expected.txt
```

每个 lab 的 README 末尾都有**练习参考答案**（先自己做，再看答案）。

## 目录结构

```
assemblylabs/
├── README.md          ← 你正在看的这个
├── WINDOWS.md         ← 平台环境 + Microsoft x64 ABI 速查
├── REFERENCES.md      ← 推荐教材和资源
├── Makefile           ← 顶层 (make all / make test)
├── common/
│   ├── asm_io.inc     ← GAS 宏库 (PRINT_INT 等, lab05 起使用)
│   └── rules.mk       ← 共享构建规则
├── lab01/ ... lab16/  ← 16 个递进实验
│   ├── README.md      ← 讲解 + 练习 + 参考答案
│   ├── *.s / *.c      ← 源码
│   ├── Makefile       ← 3 行, 引用 common/rules.mk
│   └── expected.txt   ← 自动测试基准输出
└── lab16/boot.s       ← 唯一例外: 16 位实模式引导扇区
```

## 核心约定

- 语法：**AT&T (GAS)**，通过 **gcc** 编译链接
- 平台：**Windows MSYS2 UCRT64**，Microsoft x64 ABI
- 入口点：`main`（gcc 自动处理 C 运行时初始化）
- I/O：`printf` / `puts`（走 UCRT，不直接摸系统调用）
- 64 位整数格式符用 `%lld`（Windows 的 long 是 32 位！）
- 构建：`mingw32-make`（Makefile 已设 `SHELL = sh`）

## 推荐阅读

见 `REFERENCES.md`
