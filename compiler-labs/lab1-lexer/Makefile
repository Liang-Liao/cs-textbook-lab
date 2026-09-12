# lab1: 手写词法分析器
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/lexer.c src/symtab.c src/main.c
TARGET  = minilex.exe

$(TARGET): $(SRCS) src/lexer.h src/token.h src/symtab.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
