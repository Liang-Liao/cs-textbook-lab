int base = 7;
int probe(int x) {
    int b[2];
    b[0] = x;
    b[1] = b[0] + base;
    return b[1];
}
void poke() { base = base + 1; }
int seed = probe(5);
int a[8];
int i = 0;
while (i < 8) {
    a[i] = probe(i) + seed * 2;
    i = i + 1;
}
print a[3];
probe(99);
poke();
print base;
print probe(2);
