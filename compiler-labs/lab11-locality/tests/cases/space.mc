int m[6][6];
for (int i = 1; i < 6; i = i + 1)
    for (int j = 0; j < 5; j = j + 1)
        m[i][j] = m[i-1][j+1] + i * j;
print m[5][4];
