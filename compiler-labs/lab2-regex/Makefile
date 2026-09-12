# lab2: 正则→NFA→DFA→最小化 + mini 词法生成器
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = src/re_parse.c src/nfa.c src/dfa.c src/scan.c src/main.c
HDRS    = src/re_parse.h src/nfa.h src/dfa.h src/scan.h
TARGET  = minire.exe

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
