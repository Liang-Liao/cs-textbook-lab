// LICM 演示：g 与 n 是循环不变量，循环内的 g * n 应被外提到 preheader
int g = 7;
int n = 5;
int i = 0;
int acc = 0;
while (i < n) {
    acc = acc + g * n;      // 不变量乘法 → 外提后只算一次
    i = i + 1;
}
print acc;                  // 175
print acc;                  // 175
