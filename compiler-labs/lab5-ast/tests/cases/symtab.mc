// 作用域树 dump 测试：全局/嵌套块/内层遮蔽同名变量
int a = 1;
float b = 2.5;
{
    int c = a + 1;
    {
        float a = 3.5;
        print a;
    }
    print c;
}
print a;
