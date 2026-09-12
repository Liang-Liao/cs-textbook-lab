// IR dump 测试小程序：if/while/&&/调用/遮蔽变量名唯一化，肉眼可验回填
int abs(int x) {
    if (x < 0) return -x;
    return x;
}
int main_x = 3;
{
    int main_x = -5;
    print abs(main_x);    // 5  内层遮蔽（IR 里两个不同名字）
}
print abs(main_x);        // 3
int i = 0;
int s = 0;
while (i < 4 && 1) {
    s = s + i;
    i = i + 1;
    if (s > 3) break;
}
print s;                  // 6  (0+1+2+3=6，随后 break)
print i;                  // 4
