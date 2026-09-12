// 循环展开演示：计数循环（界 8 为偶数常量、直线体、步进 +1）
// -O 后体被复制一份、每轮迭代推进两次——输出必须与不展开时逐字节一致
int sum = 0;
for (int i = 0; i < 8; i = i + 1)
    sum = sum + i;
print sum;                  // 28

int sq = 0;
for (int j = 0; j < 6; j = j + 1)
    sq = sq + j * j;
print sq;                   // 0+1+4+9+16+25 = 55
