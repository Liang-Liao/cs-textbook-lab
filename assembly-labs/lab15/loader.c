/*
 * Lab 15: Shellcode 测试框架
 * Windows MSYS2 UCRT64 — 用 C 加载并执行机器码
 *
 * ⚠️ 仅用于学习目的: 理解"代码就是字节"以及 RWX 内存的危险性
 *
 * 两个 shellcode 都遵守 Windows x64 ABI (参数 RCX/RDX):
 *   - 无参版:   mov eax,42; ret              (经典 6 字节)
 *   - 带参版:   mov eax,ecx; add eax,edx; ret
 *     (Linux SysV 上参数在 RDI/RSI, 同样的字节会算错 — 平台差异活教材)
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* 用字节数组而不是字符串字面量: sizeof 恰好等于字节数
 * (字符串字面量会多一个 '\0', 导致 memcpy 多复制 1 字节) */
unsigned char sc_ret42[] = { 0xB8, 0x2A, 0x00, 0x00, 0x00, 0xC3 };   /* mov eax,42; ret */
unsigned char sc_add[]   = { 0x8B, 0xC1, 0x03, 0xC2, 0xC3 };         /* mov eax,ecx; add eax,edx; ret */

/* 分配 RWX 可执行内存; 失败返回 NULL */
static void *alloc_exec(size_t size) {
    void *mem = NULL;
#ifdef _WIN32
    mem = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                       PAGE_EXECUTE_READWRITE);
#else
    mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) mem = NULL;
#endif
    return mem;
}

int main() {
    printf("sc_ret42 size: %zu bytes\n", sizeof(sc_ret42));

    void *mem = alloc_exec(sizeof(sc_ret42));
    if (!mem) {
        perror("alloc_exec");
        return 1;
    }
    memcpy(mem, sc_ret42, sizeof(sc_ret42));

    printf("Executing shellcode...\n");
    int (*func)() = (int (*)())mem;
    int result = func();
    printf("Shellcode returned: %d\n", result);

#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, sizeof(sc_ret42));
#endif

    /* 带参 shellcode: 同一块 RWX 内存复用 */
    printf("\nsc_add size: %zu bytes\n", sizeof(sc_add));
    mem = alloc_exec(sizeof(sc_add));
    if (!mem) {
        perror("alloc_exec");
        return 1;
    }
    memcpy(mem, sc_add, sizeof(sc_add));

    int (*func2)(int, int) = (int (*)(int, int))mem;
    printf("func(40, 2) = %d\n", func2(40, 2));
    printf("func(100, 200) = %d\n", func2(100, 200));

#ifdef _WIN32
    VirtualFree(mem, 0, MEM_RELEASE);
#else
    munmap(mem, sizeof(sc_add));
#endif

    return 0;
}
