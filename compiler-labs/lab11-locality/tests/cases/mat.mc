int m[3][4];
int t[4][3];
int p[3][3];
for (int i = 0; i < 3; i = i + 1)
    for (int j = 0; j < 4; j = j + 1)
        m[i][j] = i * 10 + j;
for (int i = 0; i < 3; i = i + 1)
    for (int j = 0; j < 4; j = j + 1)
        t[j][i] = m[i][j];
for (int i = 0; i < 3; i = i + 1)
    for (int j = 0; j < 3; j = j + 1) {
        int s = 0;
        for (int k = 0; k < 4; k = k + 1)
            s = s + m[i][k] * t[k][j];
        p[i][j] = s;
    }
for (int i = 0; i < 3; i = i + 1)
    print p[i][0] + p[i][1] + p[i][2];
print t[3][2] * 100 + t[0][3];
