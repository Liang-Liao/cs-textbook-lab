int leaf() { return 42; }
int mid() { return leaf(); }
int fact(int n) { if (n < 2) { return 1; } return n * fact(n - 1); }
print mid();
print fact(5);
