// swapcall.mc —— 交换的调用门禁：体含 CALL 时实例顺序随交换改变
// （bump 经 helper 保持非叶、不被内联，调用原样留在体内；返回值随
// 调用序递增），各元素拿到的序号会从行主序 1..6 变成列主序。
// 门禁必须拒绝（交换 0）。
int calls = 0;
int helper() { return 0; }
int bump() { calls = calls + 1; return calls + helper(); }
int m[2][3];
for (int i = 0; i < 2; i = i + 1)
    for (int j = 0; j < 3; j = j + 1)
        m[i][j] = bump();
print m[0][0];
print m[0][1];
print m[0][2];
print m[1][0];
print m[1][1];
print m[1][2];
