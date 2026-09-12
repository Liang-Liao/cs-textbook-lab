/*
 * rt.c —— MiniC 原生运行时（lab8）
 *
 * 生成出来的汇编只包含 minicc 自己的函数；打印、除零报告、进程引导
 * 这几件"每个程序都要"的事，由这份小小的 C 运行时库提供——真实
 * 编译器同样自带运行时（libc/crt0 就是它的放大版）。
 *
 * 为什么不直接调 printf？printf 是**可变参数**函数：Windows x64 约定
 * 可变参调用要把浮点实参同时放进对应的通用寄存器、还要在 AL 里报告
 * 用了几个向量寄存器。让生成代码背这些细节太吵了——包成固定签名的
 * 助手，调用约定立刻回到最朴素的形状。这也是"编译器作者选择运行时
 * 接口"的一个真实例子：接口越窄，后端越简单。
 *
 * 链接方式（见 tests/run_tests.sh）：
 *   gcc prog.s src/rt.c -o prog.exe
 */
#include <stdio.h>
#include <stdlib.h>

void minic_print_int(long v)     { printf("%ld\n", v); }
void minic_print_float(double v) { printf("%g\n", v); }

/* 整数除零/取模零：MiniC 语义 = 带行号报错并停机（退出码非 0）。
 * 生成代码在每条 idiv 前检查除数、非零跳过；为零则带着源行号进来
 * （第一个参数走 %ecx），本函数不返回。 */
void minic_div_zero(int line)
{
    fprintf(stderr, "line %d: runtime error: 整数除法除数为零\n", line);
    exit(1);
}

void minic_mod_zero(int line)
{
    fprintf(stderr, "line %d: runtime error: 整数取模除数为零\n", line);
    exit(1);
}

/* 进程引导：MiniC 的顶层语句编译进 mc_top 段（名字避开用户的 main
 * 函数）；C 的 main 只负责调它一下——相当于 crt0 里"把控制权交给
 * 语言入口"的那一步。全局变量是 .comm 零初始化，天然就绪。 */
int main(void)
{
    extern void mc_top(void);
    mc_top();
    return 0;
}
