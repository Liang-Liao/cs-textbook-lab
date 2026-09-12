// 数组测试（龙书 6.4）：零初始化、下标读写、表达式下标、浮点数组、
// 元素作实参（值传递）、块级作用域、递归激活各拥其帧
int add(int a, int b) { return a + b; }

int g[5];                      // 全局数组，声明即零初始化
print g[0];                    // 0
print g[4];                    // 0

g[0] = 10;
g[g[0] / 5 - 1] = 7;           // 下标是表达式：即 g[1] = 7
print g[0] + g[1];             // 17

int i = 0;
while (i < 5) {                // 循环填充：g = 100..500
    g[i] = (i + 1) * 100;
    i = i + 1;
}
print g[0];                    // 100
print g[3];                    // 400
print g[4];                    // 500

print add(g[1], g[2]);         // 200 + 300 = 500（元素作实参，传的是值）
g[2] = add(g[0], 5);           // 函数返回值写回元素
print g[2];                    // 105

// 块内局部数组；下标本身又是数组元素
{
    int sq[8];
    i = 0;
    while (i < 8) {
        sq[i] = i * i;
        i = i + 1;
    }
    print sq[7];               // 49
    print sq[sq[2]];           // sq[4] = 16
}

// 浮点数组：int 写入自动提升（同标量的赋值规则）
float fs[3];
fs[0] = 1;
fs[1] = 2.5;
fs[2] = fs[0] + fs[1];
print fs[2];                   // 3.5
print fs[1] * 2;               // 5

// 递归中局部数组各自成帧——若所有激活共享一份存储，结果必错：
// probe(n) 在自己的 slot[n%3] 写入 n*100，深层递归返回后本层格子必须原样
int probe(int n) {
    int slot[3];
    slot[n % 3] = n * 100;
    if (n == 0) return slot[0];
    int r = probe(n - 1);
    return r + slot[n % 3];
}
print probe(4);                // 400+300+200+100+0 = 1000
