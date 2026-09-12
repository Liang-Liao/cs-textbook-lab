int a[16];
for (int i = 1; i < 16; i = i + 1)
    a[i] = a[i-1] + 1;
print a[15];
int b[8];
for (int i = 0; i < 7; i = i + 1)
    b[i] = b[i+1] * 2 + 1;
print b[0];
int c[8];
for (int i = 0; i < 8; i = i + 1)
    c[i] = c[i] / 2 + 1;
print c[3];
int m[4][4];
for (int i = 0; i < 4; i = i + 1)
    for (int j = 0; j < 4; j = j + 1)
        m[i][j] = m[i][j] * 2 + i + j;
print m[3][3];
int u[9];
int k = 3;
for (int i = 0; i < 9; i = i + 1)
    u[i] = u[k] + i;
print u[8];
