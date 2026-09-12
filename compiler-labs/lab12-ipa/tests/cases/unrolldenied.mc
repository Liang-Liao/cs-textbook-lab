int a[20];
a[1] = 1;
for (int i = 2; i < 20; i = i + 1)
    a[i] = a[i-1] * 3;
print a[19];
