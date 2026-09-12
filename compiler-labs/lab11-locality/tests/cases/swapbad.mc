int m[4][4];
m[0][0] = 1;
m[0][1] = 2;
m[0][2] = 3;
for (int i = 1; i < 4; i = i + 1)
    for (int j = 0; j < 3; j = j + 1)
        m[i][j] = m[i-1][j+1] * 2;
print m[1][0] + m[3][2] + m[2][1];
