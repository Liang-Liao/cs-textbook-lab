int base = 10;
int addk(int a, int b) { return a + b + base; }
void shout(int n) { print n + 100; }
int twice(int x) { return x * 2; }
print addk(3, 4);
print addk(base, twice(6));
shout(addk(1, 1));
print twice(twice(5));
