// MiniC v1 词法演示程序：覆盖 lab1 支持的全部记号类别
/* 块注释：
   可以跨行，也可以包含 * 星号和 / 斜杠 */
int radius = 10;
float pi = 3.14, area;

area = 3.14 * radius * radius;

if (area >= 300.0) {
    print area;      // 条件成立时打印
} else {
    print 0;
}

while (radius != 0) {
    radius = radius - 1;
}

void _helper() { return; }
int arr[10];
// 下面这行验证最长匹配：<= 是一个记号，不是 < 和 =
int cmp = (area <= 500) + (area > 5) + (area < 6) + (1 == 1) + (2 != 3);
int logic = !cmp && cmp || 0;
break; continue;
