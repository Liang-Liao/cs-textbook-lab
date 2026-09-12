// unrollinit.mc —— 展开初值门禁 + for 赋值式 init 的回归钉。
// [1] 赋值式 init：parse_for 必须自己补吃 init 的分号（曾把 cond 吃丢）；
//     奇数起点 ⇒ 趟数 = N − s 为奇数 ⇒ 盲目展开会让体以 i==N 多跑一次，
//     门禁必须拒绝（正确 s=28，被错误展开会得到 36）。
// [2] 零起点偶数趟：合法形态，展开应照常生效。
int s = 0;
int i = 0;
for (i = 1; i < 8; i = i + 1) s = s + i;
print s;
int t = 0;
for (int k = 0; k < 8; k = k + 1) t = t + k;
print t;
