# lab0: 中缀转后缀翻译器
# 构建: mingw32-make   (或 gcc -std=c11 -Wall -Wextra -O2 -o infix2postfix.exe src/infix2postfix.c)

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
TARGET  = infix2postfix.exe

$(TARGET): src/infix2postfix.c
	$(CC) $(CFLAGS) -o $(TARGET) src/infix2postfix.c

clean:
	rm -f $(TARGET)

.PHONY: clean
