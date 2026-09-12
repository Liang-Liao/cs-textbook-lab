// swapinit.mc —— 空 init 的内层循环：起点未知就不得交换。内层 j 从
// 2 起步全程只跑 2 次（i=1 时 j 已是 4）；被错误交换会强写 j=0
// 把计数器清零重跑成 8 次。空 init 直接放弃（交换 0）。
int s = 0;
int i;
int j = 2;
for (i = 0; i < 2; i = i + 1)
    for (; j < 4; j = j + 1)
        s = s + 1;
print s;
