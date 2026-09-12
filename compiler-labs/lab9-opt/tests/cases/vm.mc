// vm.mc —— 显式活动记录的硬证据（lab7 新增测试集）
//
// 这些程序在 lab6 就能跑对，但它们专门构造出"若把所有 IR 名放进一个
// 扁平名字环境就必错"的形状——递归时同一个名字（如 down 的 pre）在
// 栈上同时存在多份，每份住各层自己的帧里。栈帧 VM 的正确性全靠
// "同名不同帧"，这一组用例就是对它的直接检验。

// 1) 递归中"先算好、递归后再用"的局部变量：pre 在嵌套调用返回后必须
//    还是本层那份。扁平环境里会被更深层覆写成 10（最深层最后写入的值）
int down(int n) {
    if (n == 0) return 1;
    int pre = n * 10;          // 本层先算好
    int rest = down(n - 1);    // 深层会执行很多次 pre = ...，但碰不到本层的
    return pre + rest;
}
print down(4);                 // 40+30+20+10+1 = 101

// 2) 累加式递归：每层的 s 独立成活，加总顺序无关错
int sum(int n) {
    if (n == 0) return 0;
    int s = n + sum(n - 1);
    return s;
}
print sum(5);                  // 15

// 3) 参数按值：被调方改的是自己帧里的形参槽，实参分毫不动
int try_set(int x) {
    x = 99;
    return x;
}
int v = 5;
print try_set(v);              // 99
print v;                       // 5   （值传递的铁证）

// 4) 全局 vs 帧内局部：函数读写全局共享一份；形参与全局同名时各归各
int g = 100;
void touch() { g = g + 1; }
int shadow(int g) { g = g * 2; return g; }   // 形参遮蔽全局名
print g;                       // 100
touch();
touch();
print g;                       // 102  （两次调用改的都是全局这份）
print shadow(g);               // 204  （翻倍的是它自己帧里的 g）
print g;                       // 102  （全局安然无恙）

// 5) 深递归 + 跨调用存活：栈上同时压着 depth 个完整活动记录
int depth(int n) {
    if (n == 0) return 0;
    int mine = n;
    int r = depth(n - 1);
    return r + mine;           // 回来时 mine 必须还是本层的 n
}
print depth(100);              // 5050

// 6) 相互穿插的调用与返回值传递（caller 清栈/acc 复用的压力测试）
int add(int a, int b) { return a + b; }
print add(add(1, add(2, add(3, 4))), add(5, 6));   // 21
