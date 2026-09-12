// Lab 10: C 调用汇编函数
// Windows MSYS2 UCRT64 — Microsoft x64 ABI
//
// ⚠️ LLP64: Windows 上 long 是 32 位, 与汇编侧的 8 字节元素不匹配,
//    所以这里用 long long (格式串 %lld)。Linux 上两者都是 long + %ld。

#include <stdio.h>
#include <string.h>

extern size_t asm_strlen(const char *str);
extern long long asm_max(long long a, long long b);
extern long long asm_dot_product(const long long *arr1, const long long *arr2, size_t len);

int main() {
    // 测试 asm_strlen
    const char *test_str = "Hello, Assembly!";
    printf("C strlen:    %zu\n", strlen(test_str));
    printf("asm_strlen:  %zu\n", asm_strlen(test_str));

    // 测试 asm_max
    printf("\nasm_max(10, 20) = %lld\n", asm_max(10, 20));
    printf("asm_max(50, 30) = %lld\n", asm_max(50, 30));

    // 测试 asm_dot_product
    long long arr1[] = {1, 2, 3, 4, 5};
    long long arr2[] = {2, 3, 4, 5, 6};
    printf("\nasm_dot_product: %lld\n", asm_dot_product(arr1, arr2, 5));

    // 内联汇编 ("r" 约束让编译器自己挑寄存器, 两个平台都成立)
    long long a = 100, b = 200, sum;
    asm volatile (
        "movq %1, %0\n\t"
        "addq %2, %0"
        : "=r" (sum)
        : "r" (a), "r" (b)
    );
    printf("\nInline asm: %lld + %lld = %lld\n", a, b, sum);

    return 0;
}
