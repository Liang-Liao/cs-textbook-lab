// 类型/语义错误测试：6 类错误都应报出且退出码为 1，程序不执行
int x = 1;
int x = 2;          // 1. 重复声明
print y;            // 2. 使用未声明变量
int a = 1.5;        // 3. float 赋给 int（隐式收窄被禁止）
float b = 2.5;
print b % 2;        // 4. 浮点取模
break;              // 5. break 在循环外
print 1 +;          // 6. 语法错误
