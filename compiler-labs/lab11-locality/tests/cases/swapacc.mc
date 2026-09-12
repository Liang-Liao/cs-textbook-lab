// swapacc.mc —— 交换的标量门禁：浮点累加器 acc 携带跨迭代流依赖，
// 依赖分析只看数组访存看不见它；交换会把行主累加改序成列主，
// IEEE 加法不结合时输出可观察改变。标量进体必须拒绝（交换 0）。
float acc = 0.0;
for (int i = 0; i < 3; i = i + 1)
    for (int j = 0; j < 4; j = j + 1)
        acc = acc + i * 4 + j * 0.5;
print acc;
