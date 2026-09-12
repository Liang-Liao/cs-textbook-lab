/*
 * Lab 10: C 与汇编互操作
 * Windows MSYS2 UCRT64 — Microsoft x64 ABI
 *
 * C 侧声明与汇编侧实现的约定必须一致:
 *   参数: RCX, RDX, R8 ...   返回值: RAX
 *   callee 必须保护: RBX, RBP, RDI, RSI, R12-R15
 *
 * ⚠️ LLP64 差异: Windows 上 long 是 32 位 (Linux 上 64 位)!
 *    本 lab 的数组元素是 8 字节, 所以 C 侧用 long long, 格式串用 %lld
 */

.section .text

/* ============================================
 * asm_strlen: 计算字符串长度 (repnz scasb 版, 与 lab07 的逐字节版对比)
 * RCX = const char* -> RAX = len
 * ============================================ */
.globl asm_strlen
asm_strlen:
    push %rdi                  /* RDI 是 callee-saved, 且 scas 固定用它扫描 */
    mov %rcx, %rdi
    xor %eax, %eax             /* 找 '\0' */
    or $-1, %rcx               /* RCX = -1: 扫描计数上限 */
    repnz scasb                /* 逐字节扫描直到找到 0; 每字节 RCX-1 */
    not %rcx                   /* RCX = len + 2 */
    lea -1(%rcx), %rax         /* RAX = len */
    pop %rdi
    ret

/* ============================================
 * asm_max: 返回较大值
 * RCX = a, RDX = b -> RAX = max
 * ============================================ */
.globl asm_max
asm_max:
    mov %rcx, %rax
    cmp %rdx, %rax
    jge .max_done
    mov %rdx, %rax
.max_done:
    ret

/* ============================================
 * asm_dot_product: 点积 (叶子函数)
 * RCX = arr1*, RDX = arr2*, R8 = len -> RAX = sum
 * ============================================ */
.globl asm_dot_product
asm_dot_product:
    xor %eax, %eax
    xor %r9d, %r9d             /* i = 0 (RCX/RDX 已是参数) */
.dot_loop:
    cmp %r8, %r9
    jge .dot_done
    mov (%rcx,%r9,8), %r10
    imul (%rdx,%r9,8), %r10
    add %r10, %rax
    inc %r9
    jmp .dot_loop
.dot_done:
    ret
