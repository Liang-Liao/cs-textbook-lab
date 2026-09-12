// 优化演示：分别运行 minicc9 -ir 与 minicc9 -ir -O，diff 即本 lab 的主演示
int g = 10;
int add(int a, int b) { return a + b; }
int cse(int a, int b) {
    int s = a * b;             // 第一次 a*b
    return s + a * b;          // 第二次命中 CSE，直接复用 s
}
// 1) 常量折叠：整条表达式变成一个立即数
print 2 + 3 * 4;               // 14
// 2) 常量传播 + 折叠链：x 的每个使用都换成 10 再折掉
int x = 10;
print x * 2 + x;               // 30
print x < 20;                  // 1（比较也折叠）
// 3) 局部 CSE：操作数非常量时，相同子表达式只算一次
print cse(3, 4);               // 24
// 4) 死代码：裸表达式的结果没人用 → 连同其临时一起被删
1 + 1;
// 5) 调用边界的保守性：CALL 清空全部缓存，但全局读取依旧正确
print g + add(1, 2);           // 13
print g;                       // 10
// 6) 控制流：循环回边 + break，供窥孔清理冗余跳转
int i = 0;
while (i < 3) {
    i = i + 1;
    if (i == 2) break;
}
print i;                       // 2
