// foldcmp.mc —— 折叠器的整型比较必须按 long 精确比：|v|>2^53 后
// 经 double 中转分不出相差 1 的两个乘积，结论会与 VM 的整型整比
// （vm.c compare）和原生 cmpq 相反，带 -O 就改变输出。修复后应得 1/0。
int a = 2147483647;
print a * a > (a - 1) * (a + 1);
print a * a == (a - 1) * (a + 1);
