# 在 MSYS2 UCRT64 上运行（主平台文档）

本课程**原生运行在 Windows**，通过 Microsoft x64 ABI 直接调用 UCRT 的 C 库。
所有 16 个 lab 在本平台构建、运行并通过自动化测试。

## 环境准备

打开 **MSYS2 UCRT64** 终端（不是 MSYS2，不是 MINGW64，是 UCRT64）：

```bash
# 确认工具链
gcc --version
as --version
gdb --version

# 缺什么装什么
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils
pacman -S mingw-w64-ucrt-x86_64-make          # mingw32-make
pacman -S mingw-w64-ucrt-x86_64-gdb           # 调试器
pacman -S mingw-w64-ucrt-x86_64-qemu          # 仅 lab16 需要
```

构建命令是 **`mingw32-make`**（UCRT64 的 make 叫这个名字）。
各 Makefile 已设 `SHELL = sh`，配方在 sh 里执行。

## Microsoft x64 调用约定（课程唯一 ABI）

```
整型参数:   RCX, RDX, R8, R9 (第 5 个起走栈, 栈槽在 shadow space 之上)
浮点参数:   值放 XMM0-XMM3; ⚠️ variadic 函数(printf)还要求
            把位模式复制到对应位置的整型寄存器 (lab09 实测验证)
返回值:     RAX / XMM0
caller-saved (调用后被破坏):   RAX, RCX, RDX, R8-R11, XMM0-XMM5
callee-saved (必须保护原值):   RBX, RBP, RDI, RSI, R12-R15, XMM6-XMM15
```

### 三条硬规则

1. **shadow space**：每次调用前预留 32 字节（`sub $32, %rsp` 或并入更大的 sub）
2. **栈对齐**：call 执行前 RSP 必须 16 字节对齐（被调方入口时 RSP ≡ 8 mod 16）
3. **保护 RDI/RSI**：Windows 上它们是 callee-saved（Linux System V 里是易失的）！

### 栈帧模板（课程统一写法）

```
进函数:        RSP ≡ 8 (mod 16)   ← call 压了返回地址
push %rbp:     RSP ≡ 0
再 push 奇数个: RSP ≡ 8  →  sub $40   (32 shadow + 8 补齐)
再 push 偶数个: RSP ≡ 0  →  sub $32   (纯 shadow)
返回前逆序恢复 (add / pop / pop / ret)
```

### 与 System V (Linux) 的差异对照

| 方面 | Linux System V | Windows x64 | 课程标注 |
|------|----------------|-------------|----------|
| 整型参数 | RDI, RSI, RDX, RCX, R8, R9 | RCX, RDX, R8, R9 | lab01 |
| RDI/RSI 归属 | caller-saved | **callee-saved** | lab02/07 |
| 浮点参数 | 只走 XMM0-7 | XMM0-3 **+ 整型寄存器复制位模式** (variadic) | lab09/12 |
| long 宽度 | 8 字节 (LP64), `%ld` | **4 字节 (LLP64), 用 long long + `%lld`** | lab02/10 |
| variadic AL | AL = 用到的 XMM 数 | 同左 (UCRT printf 实际按整型槽位读) | lab09 |
| 栈传参位置 | 第 7 个整型参数起 | 第 5 个起 | lab12 |
| 二进制格式 | ELF (readelf) | PE/COFF (objdump -x) | lab14 |
| 引导工具链 | ld -m elf_i386 | ld 只认 PE → as --32 + objcopy 直出 | lab16 |

## 编译运行

```bash
cd lab01
mingw32-make build    # gcc -g -Wa,-I../common hello.s -o hello.exe
mingw32-make run
mingw32-make test     # 与 expected.txt 比对
```

顶层：

```bash
mingw32-make all      # 全部构建
mingw32-make test     # 全部自动验证 (16 个 PASS)
mingw32-make clean
```

## 设计要点

- **gcc 编译链接**（不是直接用 ld）：自动挂 C 运行时，入口点用 `main`，
  I/O 走 `printf`/`puts`，用 `ret` 返回
- **宏库 `common/asm_io.inc`**（lab05 起引入）：`PRINT_INT` 等宏封装了
  装参数 + shadow space + 清 AL 全部样板；宏是汇编期文本替换，
  GDB 单步能看到展开后的每条指令
- **共享规则 `common/rules.mk`**：各 lab 的 Makefile 只有 3-10 行
- **lab16 特例**：16 位实模式引导扇区不依赖 ABI，`as --32` +
  `objcopy -O binary` 直出 512 字节（UCRT64 的 ld 不支持 ELF 链接）

## 常见问题

| 症状 | 原因 | 解决 |
|------|------|------|
| `gcc: command not found` | 用了 MSYS2/MINGW64 终端 | 换 **UCRT64** 终端 |
| `make: command not found` | UCRT64 里叫 mingw32-make | `pacman -S mingw-w64-ucrt-x86_64-make` |
| printf 浮点输出 0.000000 | 没把位模式复制进 RDX | 见 lab09 的 `movq %xmm0, %rdx` |
| 打印 64 位数不对 | 用了 `%ld`（32 位） | 用 `%lld` |
| 程序在 printf 处崩溃 | 栈没 16 对齐 / 没留 shadow space | 检查栈帧模板 |
