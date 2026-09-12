// 函数/布尔相关的类型与语义错误：每类报错一次且退出码 1，程序不执行
int f(int a, float b) { return a; }
int f(int a) { return a; }        // 1. 函数重复定义（签名不同也算重名）
int g;
int g(int a) { return a; }        // 2. 变量与函数同名冲突
print h(1);                       // 3. 调用未定义函数
print f(1);                       // 4. 实参个数不匹配（需要 2 个）
print f(2.5, 1.0);                // 5. int 形参收 float 实参（收窄禁止）
void v() { print 1; }
print v() + 1;                    // 6. void 调用当值用
return 1;                         // 7. 顶层 return
int w() { print 1; }
print 1 && v();                   // 8. void 调用做短路操作数
print 2 && ;                      // 9. 语法错误（缺右操作数）
