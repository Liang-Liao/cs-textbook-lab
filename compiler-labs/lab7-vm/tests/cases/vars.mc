// MiniC v4 测试程序：变量、类型提升、if/else、while、break/continue、
// 块作用域遮蔽、未初始化零值。每行注释给出期望输出。
int x = 3;
float y = 2.5;
print x;            // 3
print y;            // 2.5
x = x + 1;
print x;            // 4
// 隐式提升：float 变量 = int 表达式（AST 里插入 i2f）
y = x * 2;
print y;            // 8
// if/else
if (x > 3) print 1; else print 0;   // 1
// while 累加 1..5
int s = 0;
int i = 1;
while (i <= 5) {
    s = s + i;
    i = i + 1;
}
print s;            // 15
// break / continue：s 累加 1,2,4（跳过 3，遇 5 跳出）
i = 0;
while (1) {
    i = i + 1;
    if (i == 3) continue;
    if (i == 5) break;
    s = s + i;
}
print i;            // 5
print s;            // 15+1+2+4 = 22
// 块作用域遮蔽：内层 float a 遮蔽外层 int a，出块恢复
int a = 1;
{
    float a = 9.5;
    print a;        // 9.5
    a = a + 0.5;
    print a;        // 10
}
print a;            // 1（外层不受影响）
// 浮点混合运算
float f = 1.5;
print f * 2 + 1;    // 4
print 7 / 2;        // 3   (int/int 截断)
print 7 / 2.0;      // 3.5 (提升)
// 未初始化变量取零值（本 lab 的规定，见 README）
int z;
print z;            // 0
float w;
print w;            // 0
// 裸表达式语句与空语句照常工作
2 + 2;
;
