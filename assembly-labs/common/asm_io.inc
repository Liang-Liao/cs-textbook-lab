/*
 * common/asm_io.inc — Windows x64 (Microsoft ABI) 输出宏库
 *
 * 用法: 在 .text 段之后写  .include "asm_io.inc"
 *       (Makefile 已通过 -Wa,-I../common 提供搜索路径)
 *
 * 为什么要用宏?
 *   每次调 printf/puts 都要装参数、留 shadow space、清 AL,
 *   样板代码约 4-6 行。宏把这些固化成一行, 源码聚焦在"学什么"上。
 *
 * ⚠️ 前提: 调用宏时 RSP 必须 16 字节对齐。
 *   按课程统一的函数序言模板写 (见 WINDOWS.md) 即可保证:
 *     push %rbp / push callee-saved / sub $N (补齐到 16 对齐 + 32 shadow)
 *
 * 每个宏展开后做的事 (Windows x64 调用约定要点):
 *   1. 整型参数装入 RCX, RDX, R8, ...
 *   2. sub $32 预留 shadow space, 调用后释放
 *   3. variadic 函数 (printf) 把 AL 设为用到的 XMM 寄存器个数
 *   4. 浮点参数: 值放 XMM0, 且位模式复制进整型参数寄存器
 *      (UCRT printf 实际从整型寄存器的 home 槽位读取 — 见 PRINT_DBL)
 */

/* PRINT_STR str — puts(str): 打印字符串自带换行 (非 variadic, 无需设 AL) */
.macro PRINT_STR str
    lea \str(%rip), %rcx        /* 第 1 参数 -> RCX */
    sub $32, %rsp               /* shadow space */
    call puts
    add $32, %rsp
.endm

/* PRINT_INT fmt, val — printf(fmt, val): 传 1 个 64 位值 (整数用 %lld, 地址/%s 直接给寄存器) */
.macro PRINT_INT fmt, val
    lea \fmt(%rip), %rcx        /* 第 1 参数: 格式串 */
    mov \val, %rdx              /* 第 2 参数: 值 */
    xor %eax, %eax              /* 无浮点参数 -> AL = 0 */
    sub $32, %rsp
    call printf
    add $32, %rsp
.endm

/* PRINT_2INT fmt, val1, val2 — printf(fmt, val1, val2) */
.macro PRINT_2INT fmt, val1, val2
    lea \fmt(%rip), %rcx
    mov \val1, %rdx             /* 第 2 参数 */
    mov \val2, %r8              /* 第 3 参数 */
    xor %eax, %eax
    sub $32, %rsp
    call printf
    add $32, %rsp
.endm

/* PRINT_DBL fmt — printf(fmt, 双精度值): 值先放进 XMM0
 * ⚠️ Windows 关键差异: UCRT 的 variadic 函数从"整型参数寄存器"里
 *    读浮点位模式 (与 System V 用 XMM 传递完全不同),
 *    所以宏会把 XMM0 的位模式复制到 RDX — 实测缺了它就输出 0 */
.macro PRINT_DBL fmt
    movq %xmm0, %rdx            /* 位模式 -> 第 2 参数寄存器 */
    lea \fmt(%rip), %rcx
    mov $1, %eax                /* 按规范: AL = 用到的 XMM 个数 */
    sub $32, %rsp
    call printf
    add $32, %rsp
.endm
