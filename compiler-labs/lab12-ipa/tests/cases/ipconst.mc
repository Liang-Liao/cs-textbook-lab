int f(int k) { return k * 3 + 1; }
int g(int k) { return k - 2; }
int big(int k) {
    int acc = 0;
    int w = k * 2;
    int j = 0;
    while (j < 4) {
        acc = acc + w + j * k;
        if (acc > 500) { acc = acc - k; }
        j = j + 1;
    }
    return acc + k * k;
}
print f(7);
print f(7);
int v = 9;
print g(11);
print g(v);
print big(10);
