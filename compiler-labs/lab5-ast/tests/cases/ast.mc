// AST dump 测试：覆盖声明/赋值/i2f 提升/if-else/while/块遮蔽/裸表达式
int x = 3;
float y;
x = x + 1;
print x + 0.5;
if (x < 5) print 1; else print 2;
while (x < 5) x = x + 1;
{
    int x = 0;
    print x;
}
2 + 2;
