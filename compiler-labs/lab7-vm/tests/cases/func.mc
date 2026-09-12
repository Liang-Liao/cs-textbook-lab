// 函数测试：递归、参数传递与隐式提升、void 副作用、全局变量读写
int add(int a, int b) { return a + b; }
int fact(int n) {
    if (n <= 1) return 1;
    return n * fact(n - 1);
}
int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}
float half(float x) { return x / 2; }
void twice(int x) { print x; print x; }   // void：副作用无返回
int g = 10;
int bump() { g = g + 1; return g; }       // 读写全局
int ack(int m, int n) {                   // 嵌套递归（调用栈压力）
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}
print add(2, 3);          // 5
print fact(5);            // 120
print fib(10);            // 55
print half(3);            // 1.5  int 实参隐式提升 float
print half(2.5);          // 1.25
twice(7);                 // 7 7
print bump();             // 11
print bump();             // 12
print g;                  // 12  函数里的赋值改的是全局这份
print ack(2, 2);          // 7
// 实参是表达式（先求值再传值）、实参里有调用
print add(add(1, 2), fact(3));   // 3 + 6 = 9
print add(g, 0);          // 12
