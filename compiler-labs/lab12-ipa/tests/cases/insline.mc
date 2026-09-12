// insline.mc —— 内联必须保持 PARAM 的即时快照语义：每个实参在其
// param 处捕获。g(3) 会改写全局 x；若把形参绑定挪到 call 点统一重读，
// p 会读到"迟到"的 x=8 而输出 86；正确语义 p 捕获旧值 5 → 56。
int x = 0;
int g(int v) { x = x + v; return v * 2; }
int leaf(int p, int q) { return p * 10 + q; }
x = 5;
print leaf(x, g(3));
print x;
