// 数组的类型错误集：每类一条，全部报出且拒绝执行（stdout 无输出）
int add(int x, int y) { return x + y; }
int ok = 1;
int a[3];

print a;               // 数组必须带下标使用
a[0] = ok[1];          // 标量不能用下标访问（写路径）
print a[1.5];          // 数组下标必须是 int
int b[0];              // 数组长度必须为正
float c[2] = 1;        // 数组不能整体初始化，请逐元素赋值
print add(a, 1);       // 数组作实参 → 还是"必须带下标使用"
